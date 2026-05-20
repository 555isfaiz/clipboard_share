use std::{
    io::{self},
    net::{IpAddr, SocketAddr, SocketAddrV4},
    sync::{Arc, Mutex},
};

use tokio::{
    io::{AsyncReadExt, AsyncWriteExt},
    net::{TcpListener, TcpStream, UdpSocket},
};

use crate::clipboard::{ClipboardContent, ClipboardContentType, write};
use crate::message::{MessageType, build, parse};

#[derive(Debug)]
pub struct Connector {
    socket: Arc<UdpSocket>,
    tcp_listener: Arc<TcpListener>,
    udp_port: u16,
    peer_addrs: Mutex<Vec<SocketAddrV4>>,
}

impl Connector {
    pub async fn new(port: u16) -> io::Result<Self> {
        let socket = Arc::new(UdpSocket::bind(("0.0.0.0", port)).await?);
        let listener = Arc::new(TcpListener::bind("0.0.0.0:0").await?);
        Ok(Self {
            socket,
            tcp_listener: listener,
            udp_port: port,
            peer_addrs: Mutex::new(Vec::with_capacity(8)),
        })
    }

    pub async fn oneshot_broadcast(&self, payload: &[u8], buf_len: usize) -> io::Result<usize> {
        self.socket.set_broadcast(true)?;
        let res = self
            .socket
            .send_to(&payload[..buf_len], ("255.255.255.255", self.udp_port))
            .await;
        self.socket.set_broadcast(false)?;
        res
    }

    pub fn send_to_known(&self, payload: &[u8]) -> io::Result<()> {
        let peers = self.peer_addrs.lock().unwrap().clone();
        let to_remove = tokio::task::block_in_place(|| {
            tokio::runtime::Handle::current().block_on(async {
                let mut to_remove = vec![];
                for peer in &peers {
                    if let Err(e) = if payload.len() >= 10240 {
                        self.send_via_tcp(payload, peer).await
                    } else {
                        self.socket.send_to(payload, peer).await.map(|_| ())
                    } {
                        tracing::error!("send to {peer} failed: {e}");
                        to_remove.push(*peer);
                    }
                }
                to_remove
            })
        });
        if !to_remove.is_empty() {
            let mut peers = self.peer_addrs.lock().unwrap();
            peers.retain(|p| !to_remove.contains(p));
        }
        Ok(())
    }

    pub async fn send_via_tcp(&self, payload: &[u8], peer: &SocketAddrV4) -> io::Result<()> {
        let mut buf = vec![0u8; 1024];
        let port = self.tcp_listener.local_addr().unwrap().port().to_be_bytes();
        build(MessageType::Stream, &mut buf);
        buf.extend_from_slice(&port);

        if let Err(e) = self.socket.send_to(&buf, peer).await {
            return Err(io::Error::new(
                io::ErrorKind::BrokenPipe,
                format!("send to {peer} failed when starting stream connection: {e}"),
            ));
        }

        let (mut tcp_srteam, addr) = self.tcp_listener.accept().await.unwrap();
        let addrv4: Option<SocketAddrV4> = match addr {
            SocketAddr::V4(v4) => Some(v4),
            SocketAddr::V6(_) => None,
        };
        if peer != &addrv4.unwrap() {
            tracing::warn!("unknown peer {peer} established a TCP connection");
            return Err(io::Error::new(
                io::ErrorKind::NotConnected,
                format!("unknown peer {peer} established a TCP connection"),
            ));
        }

        tcp_srteam.write_all(payload).await?;
        tcp_srteam.shutdown().await
    }
}

pub fn start_receiver(buf_size: usize, connector: &Arc<Connector>) -> tokio::task::JoinHandle<()> {
    let cloned_connector = Arc::clone(connector);
    tokio::spawn(async move {
        let mut buf = vec![0u8; buf_size];
        loop {
            match cloned_connector.socket.recv_from(&mut buf).await {
                Ok((len, src)) => {
                    if src != cloned_connector.socket.peek_sender().await.unwrap() {
                        let packet = &mut buf[..len];
                        handle_packet(packet, src, &cloned_connector).await.unwrap();
                    }
                }
                Err(e) => {
                    tracing::error!("recv error: {e}");
                    break;
                }
            }
        }
    })
}

pub async fn online_boardcast(connector: &Connector) -> io::Result<()> {
    let mut buffer: Vec<u8> = Vec::with_capacity(16);
    let len = build(MessageType::Online, &mut buffer);
    let len_sent = connector.oneshot_broadcast(&buffer, len).await?;
    if len == len_sent {
        Ok(())
    } else {
        Err(io::Error::new(
            io::ErrorKind::WriteZero,
            format!("sent not complete, sent {len_sent} of {len} bytes"),
        ))
    }
}

async fn add_to_peer_list(src: std::net::SocketAddr, connector: &Connector) -> io::Result<()> {
    match src {
        SocketAddr::V4(v4) => {
            connector.peer_addrs.lock().unwrap().push(v4);
            tracing::info!("peer {src} added to list");
            Ok(())
        }
        SocketAddr::V6(_) => Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "failed to parse ip v4 address",
        )),
    }
}

async fn handle_packet(
    pkt: &mut [u8],
    src: std::net::SocketAddr,
    connector: &Connector,
) -> io::Result<()> {
    tracing::info!("got {} bytes from {src}", pkt.len());

    let (msg_type, head_start) = parse(pkt)?;
    let mut response = vec![];
    match msg_type {
        MessageType::Online => {
            if let Err(e) = add_to_peer_list(src, connector).await {
                tracing::error!("add to peer failed with {e}");
            }
            build(MessageType::AckOnline, &mut response);
            if let Err(e) = connector.socket.send_to(&response, src).await {
                tracing::error!("send to peer {src} failed with {e}");
            }
        }
        MessageType::AckOnline => {
            if let Err(e) = add_to_peer_list(src, connector).await {
                tracing::error!("add to peer failed with {e}");
            }
        }
        MessageType::ClipboardUpdate => {
            if let Err(e) = write(ClipboardContent {
                clipboard_type: ClipboardContentType::Text,
                bytes: pkt[head_start..].to_vec(),
            }) {
                tracing::error!("failed to write clipboard update content, error: {e}");
            }
        }
        MessageType::Stream => {
            // let port = u16::from_be_bytes(pkt[head_start..head_start + 2].try_into().unwrap());
            // let addrv4: Option<SocketAddrV4> = match src {
            //     SocketAddr::V4(v4) => Some(v4),
            //     SocketAddr::V6(_) => None,
            // };
            // let tcp_stream = TcpStream::connect(SocketAddrV4::new(*addrv4.unwrap().ip(), port)).await?;
            // let mut buf = vec![];
            // tcp_stream.read_to_end(&mut buf);
            // write_text()
            todo!()
        }
    };
    Ok(())
}
