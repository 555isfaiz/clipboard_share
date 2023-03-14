#include <unistd.h>
#include <Carbon/Carbon.h>
#include "udp_helper.h"
#include "clipboard.h"
#include "msg.h"

/*
 * Selected from:
 *   https://developer.apple.com/Library/mac/documentation/Miscellaneous/Reference/UTIRef/Articles/System-DeclaredUniformTypeIdentifiers.html
 */
#define PLAINTEXT CFSTR("public.utf8-plain-text")
#define IMAGE_PNG CFSTR("public.png")

static PasteboardRef clipboard;
extern unsigned buffer_size;
char *buffer_;

CFStringRef get_pasteboard_item_flavor(PasteboardItemID itemid)
{
	OSStatus status;
	CFArrayRef arr;

	status = PasteboardCopyItemFlavors(clipboard, itemid, &arr);
	if (status != noErr) 
    {
        errno = status;
		perror("PasteboardCopyItemFlavorData(PLAINTEXT) failed\n");
		return (void *)0;
	}

	CFStringRef type = (void *)0;
	for (int i = 0; i < CFArrayGetCount(arr); i++)
	{
		CFStringRef sName = (CFStringRef)CFArrayGetValueAtIndex(arr, i);
		if (CFStringCompare(sName, IMAGE_PNG, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
			type = IMAGE_PNG;
			break;
		}
		else if (CFStringCompare(sName, PLAINTEXT, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
			type = PLAINTEXT;
			break;
		}
	}

	CFRelease(arr);
	return type;
}

void read_local_clipboard(int *len)
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
		return;
	}

	CFStringRef type = get_pasteboard_item_flavor(itemid);

	status = PasteboardCopyItemFlavorData(clipboard, itemid, type, &data);
	if (status != noErr) 
    {
        errno = status;
		perror("PasteboardCopyItemFlavorData() failed\n");
		return;
	}

	*len = CFDataGetLength(data);
	buf = calloc(*len + 1, 1);
	memcpy(buf, CFDataGetBytePtr(data), *len);

	CFRelease(data);
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

void clipboard_monitor_loop()
{
    OSStatus status = PasteboardCreate(kPasteboardClipboard, &clipboard);
	if (status != noErr) 
	{
        errno = status;
		perror("PasteboardCreate() failed\n");
		return;
	}

	buffer_ = (char *)calloc(buffer_size, sizeof(char));
	if (!buffer_)
	{
		perror("failed to alloca buffer for pasteboard monitering.\n");
		return;
	}

	while (1)
	{
		if (PasteboardSynchronize(clipboard) & kPasteboardModified)
		{
			int cb_len = 0;
            read_local_clipboard(&cb_len);
            char send_buf[8192] = {0};
            int msg_len = gen_msg_clipboard_update(send_buf);
            memcpy(send_buf + msg_len, buffer_, cb_len);
            udp_broadcast_to_known(send_buf, msg_len + cb_len);
            // free(cb_buf);
		}
		sleep(1);
	}
}
