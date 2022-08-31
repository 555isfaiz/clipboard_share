#include <unistd.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreGraphics/CGEvent.h>
#include <CoreGraphics/CGDirectDisplay.h>
#include <Carbon/Carbon.h>
#include "udp_helper.h"
#include "mac.h"
#include "msg.h"

/*
 * Selected from:
 *   https://developer.apple.com/Library/mac/documentation/Miscellaneous/Reference/UTIRef/Articles/System-DeclaredUniformTypeIdentifiers.html
 */
#define PLAINTEXT CFSTR("public.utf8-plain-text")

static PasteboardRef clipboard;

char* read_local_clipboard(int *len)
{
    OSStatus status;
	PasteboardItemID itemid;
	CFDataRef data;
	char* buf;

	/*
	 * To avoid error -25130 (badPasteboardSyncErr):
	 *
	 * "The pasteboard has been modified and must be synchronized before
	 *  use."
	 */
	PasteboardSynchronize(clipboard);

	status = PasteboardGetItemIdentifier(clipboard, 1, &itemid);
	if (status != noErr) 
    {
        errno = status;
		perror("PasteboardGetItemIdentifier(1) failed\n");
		return NULL;
	}

	status = PasteboardCopyItemFlavorData(clipboard, itemid, PLAINTEXT, &data);
	if (status != noErr) 
    {
        errno = status;
		perror("PasteboardCopyItemFlavorData(PLAINTEXT) failed\n");
		return NULL;
	}

	*len = CFDataGetLength(data);
	buf = calloc(*len + 1, 1);
	memcpy(buf, CFDataGetBytePtr(data), *len);

	CFRelease(data);

	return buf;
}

void write_local_clipboard(char *buf, int len)
{
    OSStatus status;
	CFDataRef data;

	data = CFDataCreate(NULL, (UInt8*)buf, len);
	if (!data) 
    {
		perror("CFDataCreate failed\n");
		return;
	}

	/*
	 * To avoid error -25135 (notPasteboardOwnerErr):
	 *
	 * "The application did not clear the pasteboard before attempting to
	 *  add flavor data."
	 */
	PasteboardClear(clipboard);

	status = PasteboardPutItemFlavor(clipboard, (PasteboardItemID)data,
	                                 PLAINTEXT, data, 0);
	if (status != noErr) 
    {
		perror("PasteboardPutItemFlavor() failed\n");
	}

	CFRelease(data);
}

CGEventRef keyDownCallBack(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *userInfo)
{
    UniCharCount actualStringLength = 0;
    UniChar inputString[128];
    CGEventKeyboardGetUnicodeString(event, 128, &actualStringLength, inputString);

    CGEventFlags flag = CGEventGetFlags(event);

    // is command key down?
    if ((flag & kCGEventFlagMaskCommand) != kCGEventFlagMaskCommand) return event;

    for (size_t i = 0; i < actualStringLength; i++)
    {
        if (inputString[i] == 'x' || inputString[i] == 'c')
        {
            int cb_len = 0;
            char *cb_buf = read_local_clipboard(&cb_len);
            char send_buf[8192] = {0};
            int msg_len = gen_msg_clipboard_update(send_buf);
            memcpy(send_buf + msg_len, cb_buf, cb_len);
            udp_broadcast_to_known(send_buf, msg_len + cb_len);
            free(cb_buf);
            break;
        }
    }
    return event;
}

void clipboard_monitor_loop()
{
    OSStatus status = PasteboardCreate(kPasteboardClipboard, &clipboard);
	if (status != noErr) {
        errno = status;
		perror("PasteboardCreate() failed\n");
		return;
	}

    CFRunLoopRef theRL = CFRunLoopGetCurrent();
    CFMachPortRef keyUpEventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly, CGEventMaskBit(kCGEventKeyUp) | CGEventMaskBit(kCGEventFlagsChanged), &keyDownCallBack, NULL);
    CFRunLoopSourceRef keyUpRunLoopSourceRef = CFMachPortCreateRunLoopSource(NULL, keyUpEventTap, 0);
    CFRunLoopAddSource(theRL, keyUpRunLoopSourceRef, kCFRunLoopDefaultMode);
    CGEventTapEnable(keyUpEventTap, true);

    CFRunLoopRun();

    CFRelease(keyUpEventTap);
    CFRelease(keyUpRunLoopSourceRef);
}