/* 
 * File:   main.c
 * Author: megaboy
 *
 * Created on July 30, 2013, 4:09 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
// this is libusb, see http://libusb.sourceforge.net/
#include "usb.h"

// same as in main.c for device
#define USB_LED_OFF     0
#define USB_LED_ON      1
#define USB_DATA_OUT    2
#define USB_DATA_WRITE  3
#define USB_DATA_IN     4
#define USB_DATA_LONGOUT 5
#define USB_ROM_SIZE    6 //Like USB_DATA_OUT
#define USB_BLOCK_NUM   7
#define USB_Console_Name 8
#define USB_DUMP_BLOCK	9
#define USB_READ_ADDRESS 10
#define USB_DUMP_ROM    11

usb_dev_handle *handle = NULL;
int nBytes = 0;
//char buffer[256];
char buffer[512];
uint32_t ROMSIZE = 0;
uint32_t Block_Offset = 0;
FILE *fp;
// used to get descriptor strings for device identification 

static int
usbGetDescriptorString(usb_dev_handle *dev, int index, int langid,
                       char *buf, int buflen)
{
    char buffer[256];
    int rval, i;

    // make standard request GET_DESCRIPTOR, type string and given index 
    // (e.g. dev->iProduct)
    rval = usb_control_msg(dev,
                           USB_TYPE_STANDARD | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                           USB_REQ_GET_DESCRIPTOR, (USB_DT_STRING << 8) + index, langid,
                           buffer, sizeof (buffer), 1000);

    if (rval < 0) // error
        return rval;

    // rval should be bytes read, but buffer[0] contains the actual response size
    if ((unsigned char) buffer[0] < rval)
        rval = (unsigned char) buffer[0]; // string is shorter than bytes read

    if (buffer[1] != USB_DT_STRING) // second byte is the data type
        return 0; // invalid return type

    // we're dealing with UTF-16LE here so actual chars is half of rval,
    // and index 0 doesn't count
    rval /= 2;

    // lossy conversion to ISO Latin1 
    for (i = 1; i < rval && i < buflen; i++) {
        if (buffer[2 * i + 1] == 0)
            buf[i - 1] = buffer[2 * i];
        else
            buf[i - 1] = '?'; // outside of ISO Latin1 range
    }
    buf[i - 1] = 0;

    return i - 1;
}

static usb_dev_handle *
usbOpenDevice(int vendor, char *vendorName,
              int product, char *productName)
{
    struct usb_bus *bus;
    struct usb_device *dev;
    char devVendor[256], devProduct[256];

    usb_dev_handle * handle = NULL;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    for (bus = usb_get_busses(); bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
            if (dev->descriptor.idVendor != vendor ||
                dev->descriptor.idProduct != product)
                continue;

            // we need to open the device in order to query strings 
            if (!(handle = usb_open(dev))) {
                fprintf(stderr, "Warning: cannot open USB device: %s\n",
                        usb_strerror());
                continue;
            }

            // get vendor name 
            if (usbGetDescriptorString(handle, dev->descriptor.iManufacturer, 0x0409, devVendor, sizeof (devVendor)) < 0) {
                fprintf(stderr,
                        "Warning: cannot query manufacturer for device: %s\n",
                        usb_strerror());
                usb_close(handle);
                continue;
            }

            // get product name 
            if (usbGetDescriptorString(handle, dev->descriptor.iProduct,
                                       0x0409, devProduct, sizeof (devVendor)) < 0) {
                fprintf(stderr,
                        "Warning: cannot query product for device: %s\n",
                        usb_strerror());
                usb_close(handle);
                continue;
            }

            if (strcmp(devVendor, vendorName) == 0 &&
                strcmp(devProduct, productName) == 0)
                return handle;
            else
                usb_close(handle);
        }
    }

    return NULL;
}

int
LED_ON()
{
    nBytes = usb_control_msg(handle,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                             USB_LED_ON, 0, 0, (char *) buffer, sizeof (buffer), 5000);
    return nBytes;
}

int
LED_OFF()
{
    nBytes = usb_control_msg(handle,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                             USB_LED_OFF, 0, 0, (char *) buffer, sizeof (buffer), 5000);
    //printf ("Got %d bytes: %s\n", nBytes, buffer);
    return nBytes;
}

int
DATA_OUT(char *buffer, int len)
{
    nBytes = usb_control_msg(handle,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                             USB_DATA_OUT, 0, 0, buffer, len, 5000);
    return nBytes;
}

int DATA_WRITE()
{
    nBytes = usb_control_msg(handle,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
                             USB_DATA_WRITE, 'T' + ('E' << 8), 'S' + ('T' << 8),
                             (char *) buffer, sizeof (buffer), 5000);
}

int DATA_LONG_OUT()
{
    nBytes = usb_control_msg(handle,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                             USB_DATA_LONGOUT, 0, 0, (char *) buffer, sizeof (buffer), 5000);
    printf("Opening file\n");
    fp = fopen("test.bin", "wb");
    printf("Writing buffer to file\n");
    fwrite(buffer, sizeof (buffer[0]), sizeof (buffer) / sizeof (buffer[0]), fp);
    printf("Closing file\n");
    fclose(fp);
    printf("Received %d bytes: %s\n", nBytes, buffer);
}

int DATA_IN(char *data, int len)
{
    nBytes = usb_control_msg(handle,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
                             USB_DATA_IN, 0, 0, data, len, 5000);
    return nBytes;
}

int
Get_ROM_Size()
{
    nBytes = usb_control_msg(handle,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                             USB_ROM_SIZE, 0, 0, (char *) buffer, sizeof (buffer), 5000);
    printf("Got %d bytes: 0x%x%x%x%x\n", nBytes, buffer[0], buffer[1], buffer[2], buffer[3]);
    ROMSIZE = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3]);
    printf("ROM Size is %d Kbytes\n", ROMSIZE / 1024);
    return nBytes;
}

int Get_Console_Name()
{
    nBytes = usb_control_msg(handle,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                             USB_Console_Name, 0, 0, (char *) buffer, sizeof (buffer), 5000);
    printf("Got %d bytes: ROM console name:\t%s\n", nBytes, buffer);
    return nBytes;
}

int ROM_Dump_Block(uint32_t block_offet, char * buffer, int len)
{
    nBytes = usb_control_msg(handle,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                             USB_DUMP_BLOCK, block_offet >> 16, block_offet & 0xffff, buffer, len, 5000);
    return nBytes;


}

void
Show_Usage()
{
    printf("Usage:\n");
    printf("usbtext.exe on\n");
    printf("usbtext.exe off\n");
    printf("usbtext.exe out\n");
    printf("usbtext.exe write\n");
    printf("usbtext.exe in <string>\n");
    printf("usbtext.exe longout\n");
    exit(1);
}

/*
 * 
 */
int
main(int argc, char** argv)
{


    if (argc < 2) {
        Show_Usage();

    }

    handle = usbOpenDevice(0x16c0, "megaboy", 0x05dc, "Genesis ROM Dumper");

    if (handle == NULL) {
        fprintf(stderr, "Could not find USB device!\n");
        exit(1);
    }

    if (strcmp(argv[1], "on") == 0) {
        LED_ON();
    }
    else if (strcmp(argv[1], "off") == 0) {
        LED_OFF();
    }
    else if (strcmp(argv[1], "out") == 0) {
        DATA_OUT((char *) buffer, sizeof (buffer));
        printf("Got %d bytes: %s\n", nBytes, buffer);
    }
    else if (strcmp(argv[1], "write") == 0) {
        DATA_WRITE();
    }
    else if (strcmp(argv[1], "in") == 0 && argc > 2) {
        DATA_IN(argv[2], strlen(argv[2]) + 1);

    }
    else if (strcmp(argv[1], "longout") == 0) {
        DATA_LONG_OUT();

    }
    else if (strcmp(argv[1], "romsize") == 0) {
        Get_ROM_Size();

    }
    else if (strcmp(argv[1], "consolename") == 0) {

        Get_Console_Name();

    }
    else if (strcmp(argv[1], "dumpblock") == 0) {

        Get_ROM_Size();
        int percentage = 0;
        printf("Opening file\n");
        fp = fopen("test1.bin", "wb");
        printf("Writing buffer to file\n");
        for (Block_Offset = 0; Block_Offset < (ROMSIZE / sizeof (buffer)); Block_Offset++) {
            ROM_Dump_Block(Block_Offset,(char *)buffer,sizeof(buffer));

            fwrite(buffer, sizeof (buffer[0]), sizeof (buffer) / sizeof (buffer[0]), fp);
            if ((Block_Offset % 10) == 0)
                printf("\r%d", percentage);
            percentage++;
        }
        printf("\nClosing file\n");
        fclose(fp);
        printf("Received %d bytes\n", Block_Offset * 512);

    }
    else if (strcmp(argv[1], "dumprom") == 0) {

        Get_ROM_Size();
        int percentage = 0;
        printf("Opening file\n");
        fp = fopen("test1.bin", "wb");
        printf("Writing buffer to file\n");
        for (Block_Offset = 0; Block_Offset < (ROMSIZE / sizeof (buffer)); Block_Offset++) {
            nBytes = usb_control_msg(handle,
                                     USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                                     USB_DUMP_ROM, Block_Offset >> 16, Block_Offset & 0xffff, (char *) buffer, sizeof (buffer), 5000);


            fwrite(buffer, sizeof (buffer[0]), sizeof (buffer) / sizeof (buffer[0]), fp);
            if ((Block_Offset % 10) == 0)
                printf("\r%d", percentage);
            percentage++;
        }
        printf("\nClosing file\n");
        fclose(fp);
        printf("Received %d bytes\n", Block_Offset * 512);

    }
    if (nBytes < 0)
        fprintf(stderr, "USB error: %s\n", usb_strerror());

    usb_close(handle);
    return (EXIT_SUCCESS);
}

