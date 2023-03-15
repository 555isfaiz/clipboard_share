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
char *cb_buffer_;

CFStringRef get_pasteboard_item_flavor(PasteboardItemID itemid, uint8_t *type_code_out)
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
			*type_code_out = CB_TYPE_IMAGE;
			type = IMAGE_PNG;
			break;
		}
		else if (CFStringCompare(sName, PLAINTEXT, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
			*type_code_out = CB_TYPE_TEXT;
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

	uint8_t type_code = 0;
	CFStringRef type = get_pasteboard_item_flavor(itemid, &type_code);
	if (!type_code)
		return;
	
	int msg_len = gen_msg_clipboard_update(cb_buffer_);
	(cb_buffer_)[msg_len] = (char)type_code;
	msg_len++;

	status = PasteboardCopyItemFlavorData(clipboard, itemid, type, &data);
	if (status != noErr) 
    {
        errno = status;
		perror("PasteboardCopyItemFlavorData() failed\n");
		return;
	}

	*len = CFDataGetLength(data);
	if (*len <= buffer_size - msg_len) 
	{
		memcpy(cb_buffer_ + msg_len, CFDataGetBytePtr(data), *len);
		*len += msg_len;
		*len = 0;
	}

	CFRelease(data);
}

void write_local_clipboard(char *buf, int len)
{
	uint8_t type_code = buf[0];
    CFStringRef type;
    if (type_code == CB_TYPE_IMAGE) 
        type = IMAGE_PNG;
    else if (type_code == CB_TYPE_TEXT)
        type = PLAINTEXT;
    else
    {
        fprintf(stderr, "unknown type code %u.\n", type_code);
        return;
    }

    OSStatus status;
	CFDataRef data;

	data = CFDataCreate(NULL, (UInt8*)buf + 1, len - 1);
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
	                                 type, data, 0);
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

	cb_buffer_ = (char *)calloc(buffer_size, sizeof(char));
	if (!cb_buffer_)
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
			if (cb_len <= 0)
				continue;

            udp_broadcast_to_known(cb_buffer_, cb_len);
		}
		sleep(1);
	}
}