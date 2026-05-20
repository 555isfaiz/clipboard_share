use arboard::{Clipboard, ImageData};
use clipboard_master::{CallbackResult, ClipboardHandler, Master, Shutdown};
use image::GenericImageView;
use std::{
    io::{self},
    sync::{Mutex, OnceLock},
};

static CLIPBOARD: OnceLock<Mutex<Clipboard>> = OnceLock::new();

#[derive(Debug)]
pub enum ClipboardContentType {
    Text,
    Image,
    //...
}

#[derive(Debug)]
pub struct ClipboardContent {
    pub clipboard_type: ClipboardContentType,
    pub bytes: Vec<u8>,
}

struct ClipboardChangeHandler<F>
where
    F: Fn(ClipboardContent) -> io::Result<()> + Send + 'static,
{
    clipboard_broadcaster: F,
}

impl<F> ClipboardHandler for ClipboardChangeHandler<F>
where
    F: Fn(ClipboardContent) -> io::Result<()> + Send + 'static,
{
    fn on_clipboard_change(&mut self) -> CallbackResult {
        if let Ok(text) = read() {
            (self.clipboard_broadcaster)(text).unwrap();
        }
        CallbackResult::Next
    }

    fn on_clipboard_error(&mut self, error: io::Error) -> CallbackResult {
        tracing::error!("clipboard listener error: {error}");
        CallbackResult::Next
    }
}

pub fn spawn_listener<F>(f: F) -> Shutdown
where
    F: Fn(ClipboardContent) -> io::Result<()> + Send + 'static,
{
    let mut master = Master::new(ClipboardChangeHandler {
        clipboard_broadcaster: f,
    })
    .expect("clipboard master init failed");
    let shutdown = master.shutdown_channel();
    master.run().expect("Success");
    shutdown
}

fn clipboard() -> &'static Mutex<Clipboard> {
    CLIPBOARD.get_or_init(|| {
        Mutex::new(Clipboard::new().expect("failed to initialize system clipboard"))
    })
}

pub fn read() -> Result<ClipboardContent, arboard::Error> {
    let mut cb = clipboard().lock().unwrap();
    if let Ok(text_content) = cb.get_text() {
        Ok(ClipboardContent {
            clipboard_type: ClipboardContentType::Text,
            bytes: text_content.into_bytes(),
        })
    } else if let Ok(image_content) = cb.get_image() {
        Ok(ClipboardContent {
            clipboard_type: ClipboardContentType::Image,
            bytes: image_content.bytes.into_owned(),
        })
    } else {
        Err(arboard::Error::ContentNotAvailable)
    }
}

pub fn write(clipboard_content: ClipboardContent) -> Result<(), arboard::Error> {
    let mut cb = clipboard().lock().unwrap();
    match clipboard_content {
        ClipboardContent {
            clipboard_type: ClipboardContentType::Text,
            bytes,
        } => {
            let text = String::from_utf8(bytes)
                .map_err(|_| arboard::Error::ConversionFailure)?;
            cb.set_text(text)
        }
        ClipboardContent {
            clipboard_type: ClipboardContentType::Image,
            bytes,
        } => {
            let img = image::load_from_memory(&bytes).unwrap();
            let (w, h) = img.dimensions();
            let image = ImageData {
                width: w as usize,
                height: h as usize,
                bytes: bytes.into(),
            };
            cb.set_image(image)
        },
    }
}
