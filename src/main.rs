mod clipboard;
mod connection;
mod message;

use std::sync::Arc;

use clap::Parser;
use connection::Connector;
use message::{build, set_token};
use tracing::info;
use tracing_subscriber::EnvFilter;

use crate::{
    connection::{online_boardcast, start_receiver},
    message::MessageType,
};

#[derive(Parser, Debug)]
#[command(version, about)]
struct Args {
    /// Port to listen on
    #[arg(short = 'p', default_value_t = 8080)]
    port: u16,

    /// Enable verbose logging
    #[arg(short = 't')]
    token: String,

    #[arg(short = 'b', default_value_t = 1024 * 1024)]
    buffer_size: usize,
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::from_default_env())
        .init();

    let args = Args::parse();

    info!("initing...");

    set_token(args.token);
    let connector = Arc::new(Connector::new(args.port).await?);

    info!("broadcasting online msg...");
    online_boardcast(&connector).await?;

    info!("starting udp receiver...");
    let udp_receiver_joinable = start_receiver(args.buffer_size, &connector);

    info!("starting clipboard monitor loop...");
    let shundown = clipboard::spawn_listener(move |str| {
        let mut buffer: Vec<u8> = Vec::with_capacity(args.buffer_size);
        let _ = build(MessageType::ClipboardUpdate, &mut buffer);
        buffer.extend_from_slice(str.as_bytes());
        connector.send_to_known(&buffer)
    });

    info!("init done");
    udp_receiver_joinable.await.unwrap();
    shundown.signal();
    Ok(())
}
