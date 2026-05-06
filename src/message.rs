use std::io::Write;
use std::sync::OnceLock;
use std::{
    io::{self},
    str::{self},
};

static TOKEN: OnceLock<String> = OnceLock::new();

#[derive(Debug)]
pub enum MessageType {
    Online,
    AckOnline,
    ClipboardUpdate,
    Stream,
}

impl MessageType {
    fn value(&self) -> &str {
        match *self {
            MessageType::Online => "CB_MSG_ONLINE",
            MessageType::AckOnline => "CB_MSG_ACKONL",
            MessageType::ClipboardUpdate => "CB_MSG_UPDATE",
            MessageType::Stream => "CB_MSG_STREAM",
        }
    }
}

fn parse_type(strs: &str) -> io::Result<MessageType> {
    match strs {
        "CB_MSG_ONLINE" => Ok(MessageType::Online),
        "CB_MSG_ACKONL" => Ok(MessageType::AckOnline),
        "CB_MSG_UPDATE" => Ok(MessageType::ClipboardUpdate),
        "CB_MSG_STREAM" => Ok(MessageType::Stream),
        _ => Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "unknown message type",
        )),
    }
}

pub fn set_token(raw: String) {
    let mut h: u32 = 5381;
    for b in raw.bytes() {
        h = h.wrapping_mul(33).wrapping_add(b as u32);
    }
    TOKEN.set(h.to_string()).expect("set token failed");
}

fn token() -> &'static String {
    TOKEN.get().expect("failed getting token")
}

pub fn build(msg_type: MessageType, buf: &mut Vec<u8>) -> usize {
    let code = msg_type.value();
    write!(buf, "{}{}", token(), code).unwrap();
    token().len() + code.len()
}

pub fn parse(buf: &mut [u8]) -> io::Result<(MessageType, usize)> {
    let token = token();
    let head_end = token.len() + 13;
    if buf.len() < head_end {
        return Err(io::Error::new(
            io::ErrorKind::UnexpectedEof,
            "buffer too short",
        ));
    }
    let token_match = str::from_utf8(&buf[..token.len()])
        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))
        .iter()
        .any(|t| *t == token);
    if !token_match {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "token mismatch"));
    }
    str::from_utf8(&buf[token.len()..head_end])
        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))
        .and_then(parse_type)
        .map(|t| (t, head_end))
}
