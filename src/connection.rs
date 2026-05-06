use std::{
    io::{self},
    net::{SocketAddr, SocketAddrV4},
    sync::Arc,
};

use tokio::{net::UdpSocket, sync::Mutex};

use crate::clipboard::write_text;
use crate::message::{parse, build, MessageType};

#[derive(Debug)]
pub struct Connector {
    socket: Arc<UdpSocket>,
    udp_port: u16,
    peer_addrs: Mutex<Vec<SocketAddrV4>>,
}

impl Connector {
    pub async fn new(port: u16) -> io::Result<Self> {
        let socket = Arc::new(UdpSocket::bind(("0.0.0.0", port)).await?);
        Ok(Self {
            socket,
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
        if payload.len() >= 10240 {
            self.send_via_tcp(payload);
        } else {
            tokio::task::block_in_place(|| {
                tokio::runtime::Handle::current().block_on(async {
                    let peers = self.peer_addrs.lock().await;
                    for peer in peers.iter() {
                        if let Err(e) = self.socket.send_to(payload, peer).await {
                            tracing::error!("send to {peer} failed: {e}");
                        }
                    }
                });
            });
        }
        Ok(())
    }

    pub fn send_via_tcp(&self, payload: &[u8]) {
        todo!()
    }
}

pub fn start_receiver(
    buf_size: usize,
    connector: &Arc<Connector>,
) -> tokio::task::JoinHandle<()> {
    let cloned_connector = Arc::clone(connector);
    tokio::spawn(async move {
        let mut buf = vec![0u8; buf_size];
        loop {
            match cloned_connector.socket.recv_from(&mut buf).await {
                Ok((len, src)) => {
                    if src != cloned_connector.socket.peek_sender().await.unwrap() {
                        let packet = &mut buf[..len];
                        handle_packet(packet, src, &cloned_connector)
                            .await
                            .unwrap();
                    }
                }
                Err(e) => {
                    tracing::error!("recv error: {e}");
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
            connector.peer_addrs.lock().await.push(v4);
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
        MessageType::ClipboardUpdate => match str::from_utf8(&pkt[head_start..]) {
            Ok(content) => {
                if let Err(e) = write_text(content) {
                    tracing::error!(
                        "failed to write clipboard update content: {content}, error: {e}"
                    );
                }
            }
            Err(e) => tracing::error!("failed to parse clipboard update content: {e}"),
        },
        MessageType::Stream => {
            todo!()
        }
    };
    Ok(())
}
