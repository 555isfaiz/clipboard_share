use arboard::Clipboard;
use clipboard_master::{CallbackResult, ClipboardHandler, Master, Shutdown};
use std::{
    io::{self},
    sync::{Mutex, OnceLock},
};

static CLIPBOARD: OnceLock<Mutex<Clipboard>> = OnceLock::new();

struct ClipboardChangeHandler<F>
where
    F: Fn(String) -> io::Result<()> + Send + 'static,
{
    clipboard_broadcaster: F,
}

impl<F> ClipboardHandler for ClipboardChangeHandler<F>
where
    F: Fn(String) -> io::Result<()> + Send + 'static,
{
    fn on_clipboard_change(&mut self) -> CallbackResult {
        if let Ok(text) = read_text() {
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
    F: Fn(String) -> io::Result<()> + Send + 'static,
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

pub fn read_text() -> Result<String, arboard::Error> {
    clipboard().lock().unwrap().get_text()
}

pub fn write_text(s: &str) -> Result<(), arboard::Error> {
    tracing::info!("writing {s} into local clipboard");
    clipboard().lock().unwrap().set_text(s.to_owned())
}
