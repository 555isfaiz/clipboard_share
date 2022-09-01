#include <unistd.h>
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
int write_bit = 0;

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

	write_bit = 1;
	status = PasteboardPutItemFlavor(clipboard, (PasteboardItemID)data,
	                                 PLAINTEXT, data, 0);
	if (status != noErr) 
    {
		perror("PasteboardPutItemFlavor() failed\n");
	}

	CFRelease(data);
}

void clipboard_monitor_loop()
{
    OSStatus status = PasteboardCreate(kPasteboardClipboard, &clipboard);
	if (status != noErr) {
        errno = status;
		perror("PasteboardCreate() failed\n");
		return;
	}

	while (1)
	{
		if (PasteboardSynchronize(clipboard) & kPasteboardModified)
		{
			if (write_bit)
			{
				write_bit = 0;
				continue;
			}
			int cb_len = 0;
            char *cb_buf = read_local_clipboard(&cb_len);
            char send_buf[8192] = {0};
            int msg_len = gen_msg_clipboard_update(send_buf);
            memcpy(send_buf + msg_len, cb_buf, cb_len);
            udp_broadcast_to_known(send_buf, msg_len + cb_len);
            free(cb_buf);
		}
		sleep(1);
	}
}