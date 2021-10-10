// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/serial.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/delay.h>

#include "lan966x_udphs_regs.h"
#include "lan966x_usb.h"

#define LAN966X_SERIAL_DEVNAME		"ttyS"
#define LAN966X_SERIAL_MAJOR		TTY_MAJOR
#define LAN966X_SERIAL_MINOR		90
#define LAN966X_MAX_UART		2

#define EP_OUT				1
#define EP_IN				2
#define EP_INTER			3

#define UDPHS_EPTCFG_EPT_SIZE_8		0x0
#define UDPHS_EPTCFG_EPT_SIZE_16	0x1
#define UDPHS_EPTCFG_EPT_SIZE_32	0x2
#define UDPHS_EPTCFG_EPT_SIZE_64	0x3
#define UDPHS_EPTCFG_EPT_SIZE_128	0x4
#define UDPHS_EPTCFG_EPT_SIZE_256	0x5
#define UDPHS_EPTCFG_EPT_SIZE_512	0x6
#define UDPHS_EPTCFG_EPT_SIZE_1024	0x7

#define LAN_RD_(id, tinst, tcnt,		\
		gbase, ginst, gcnt, gwidth,		\
		raddr, rinst, rcnt, rwidth)		\
	readl(lan966x_port->udphs +			\
	      gbase + ((ginst) * gwidth) +		\
	      raddr + ((rinst) * rwidth))

#define LAN_WR_(val, id, tinst, tcnt,		\
		gbase, ginst, gcnt, gwidth,		\
		raddr, rinst, rcnt, rwidth)		\
	writel(val, lan966x_port->udphs +		\
	       gbase + ((ginst) * gwidth) +		\
	       raddr + ((rinst) * rwidth))

#define LAN_RMW_(val, mask, id, tinst, tcnt,	\
		 gbase, ginst, gcnt, gwidth,		\
		 raddr, rinst, rcnt, rwidth) do {	\
	u32 _v_;					\
	_v_ = readl(lan966x_port->udphs +		\
		    gbase + ((ginst) * gwidth) +	\
		    raddr + ((rinst) * rwidth));	\
	_v_ = ((_v_ & ~(mask)) | ((val) & (mask)));	\
	writel(_v_, lan966x_port->udphs +		\
	       gbase + ((ginst) * gwidth) +		\
	       raddr + ((rinst) * rwidth)); } while (0)

#define LAN_WR(...) LAN_WR_(__VA_ARGS__)
#define LAN_RD(...) LAN_RD_(__VA_ARGS__)
#define LAN_RMW(...) LAN_RMW_(__VA_ARGS__)

static const u8 devDescriptor[] = {
	/* Device descriptor */
	18,    // bLength
	USBGenericDescriptor_DEVICE,   // bDescriptorType
	0x00,   // bcdUSBL
	0x02,   //
	CDCDeviceDescriptor_CLASS,   // bDeviceClass:    CDC class code
	CDCDeviceDescriptor_SUBCLASS,   // bDeviceSubclass: CDC class sub code
	CDCDeviceDescriptor_PROTOCOL,   // bDeviceProtocol: CDC Device protocol
	64,   // bMaxPacketSize0
	0xEB,   // idVendorL
	0x03,   //
	0x24,   // idProductL
	0x61,   //
	0x10,   // bcdDeviceL
	0x01,   //
	0, // No string descriptor for manufacturer
	0x00,   // iProduct
	0, // No string descriptor for serial number
	1 // Device has 1 possible configuration
};

static u8 sConfiguration[] = {
	//! ============== CONFIGURATION 1 ===========
	//! Table 9-10. Standard Configuration Descriptor
	9,                            // bLength;              // size of this descriptor in bytes
	USBGenericDescriptor_CONFIGURATION,// bDescriptorType;    // CONFIGURATION descriptor type
	67, // total length of data returned 2 EP + Control + OTG
	0x00,
	2, // There are two interfaces in this configuration
	1, // This is configuration #1
	0, // No string descriptor for this configuration
	USB_CONFIG_SELF_NOWAKEUP,        // bmAttibutes;          // Configuration characteristics
	50,                            // 100mA
	
	//! Communication Class Interface Descriptor Requirement
	//! Table 9-12. Standard Interface Descriptor
	9,                       // Size of this descriptor in bytes
	USBGenericDescriptor_INTERFACE,// INTERFACE Descriptor Type
	0, // This is interface #0
	0, // This is alternate setting #0 for this interface
	1, // This interface uses 1 endpoint
	CDCCommunicationInterfaceDescriptor_CLASS, // bInterfaceClass
	CDCCommunicationInterfaceDescriptor_ABSTRACTCONTROLMODEL,       // bInterfaceSubclass
	CDCCommunicationInterfaceDescriptor_NOPROTOCOL,                       // bInterfaceProtocol
	0,  // No string descriptor for this interface
	
	//! 5.2.3.1 Header Functional Descriptor (usbcdc11.pdf)
	5, // bFunction Length
	CDCGenericDescriptor_INTERFACE, // bDescriptor type: CS_INTERFACE
	CDCGenericDescriptor_HEADER,    // bDescriptor subtype: Header Func Desc
	0x10,             // bcdCDC: CDC Class Version 1.10
	0x01,
	
	//! 5.2.3.2 Call Management Functional Descriptor (usbcdc11.pdf)
	5, // bFunctionLength
	CDCGenericDescriptor_INTERFACE, // bDescriptor Type: CS_INTERFACE
	CDCGenericDescriptor_CALLMANAGEMENT,       // bDescriptor Subtype: Call Management Func Desc
	0x00,             // bmCapabilities: D1 + D0
	0x01,             // bDataInterface: Data Class Interface 1
	
	//! 5.2.3.3 Abstract Control Management Functional Descriptor (usbcdc11.pdf)
	4,             // bFunctionLength
	CDCGenericDescriptor_INTERFACE, // bDescriptor Type: CS_INTERFACE
	CDCGenericDescriptor_ABSTRACTCONTROLMANAGEMENT,       // bDescriptor Subtype: ACM Func Desc
	0x00,             // bmCapabilities
	
	//! 5.2.3.8 Union Functional Descriptor (usbcdc11.pdf)
	5,             // bFunctionLength
	CDCGenericDescriptor_INTERFACE, // bDescriptorType: CS_INTERFACE
	CDCGenericDescriptor_UNION,     // bDescriptor Subtype: Union Func Desc
	0, // Number of master interface is #0
	1, // First slave interface is #1
	
	//! Endpoint 1 descriptor
	//! Table 9-13. Standard Endpoint Descriptor
	7,                    // bLength
	USBGenericDescriptor_ENDPOINT,// bDescriptorType
	0x80 | EP_INTER,     // bEndpointAddress, Endpoint EP_INTER - IN
	USBEndpointDescriptor_INTERRUPT,// bmAttributes      INT
	0x40, 0x00,              // wMaxPacketSize = 64
	0x10,             // Endpoint is polled every 16ms
	
	//! Table 9-12. Standard Interface Descriptor
	9,                     // bLength
	USBGenericDescriptor_INTERFACE,// bDescriptorType
	1, // This is interface #1
	0, // This is alternate setting #0 for this interface
	2, // This interface uses 2 endpoints
	CDCDataInterfaceDescriptor_CLASS,
	CDCDataInterfaceDescriptor_SUBCLASS,
	CDCDataInterfaceDescriptor_NOPROTOCOL,
	0,  // No string descriptor for this interface
	
	//! First alternate setting
	//! Table 9-13. Standard Endpoint Descriptor
	7,                            // bLength
	USBGenericDescriptor_ENDPOINT,// bDescriptorType
	EP_OUT,            // bEndpointAddress, Endpoint EP_OUT - OUT
	USBEndpointDescriptor_BULK,   // bmAttributes      BULK
	0x00, 0x02,                   // wMaxPacketSize = 512
	0, // Must be 0 for full-speed bulk endpoints
	
	//! Table 9-13. Standard Endpoint Descriptor
	7,                            // bLength
	USBGenericDescriptor_ENDPOINT,// bDescriptorType
	0x80 | EP_IN,             // bEndpointAddress, Endpoint EP_IN - IN
	USBEndpointDescriptor_BULK,   // bmAttributes      BULK
	0x00, 0x02,                   // wMaxPacketSize = 512
	0 // Must be 0 for full-speed bulk endpoints
};

static u8 sOtherSpeedConfiguration[] = {
	//! ============== CONFIGURATION 1 ===========
	//! Table 9-10. Standard Configuration Descriptor
	0x09,                            // bLength;              // size of this descriptor in bytes
	USBGenericDescriptor_OTHERSPEEDCONFIGURATION,    // bDescriptorType;      // CONFIGURATION descriptor type
	67,                            // wTotalLength;         // total length of data returned 2 EP + Control
	0x00,
	0x02, // There are two interfaces in this configuration
	0x01, // This is configuration #1
	0x00, // No string descriptor for this configuration
	USB_CONFIG_SELF_NOWAKEUP,        // bmAttibutes;          // Configuration characteristics
	50,   // 100mA
	
	//! Communication Class Interface Descriptor Requirement
	//! Table 9-12. Standard Interface Descriptor
	9,                       // Size of this descriptor in bytes
	USBGenericDescriptor_INTERFACE,// INTERFACE Descriptor Type
	0, // This is interface #0
	0, // This is alternate setting #0 for this interface
	1, // This interface uses 1 endpoint
	CDCCommunicationInterfaceDescriptor_CLASS, // bInterfaceClass
	CDCCommunicationInterfaceDescriptor_ABSTRACTCONTROLMODEL,       // bInterfaceSubclass
	CDCCommunicationInterfaceDescriptor_NOPROTOCOL,                       // bInterfaceProtocol
	0x00, // No string descriptor for this interface
	
	//! 5.2.3.1 Header Functional Descriptor (usbcdc11.pdf)
	5, // bFunction Length
	CDCGenericDescriptor_INTERFACE, // bDescriptor type: CS_INTERFACE
	CDCGenericDescriptor_HEADER,    // bDescriptor subtype: Header Func Desc
	0x10,             // bcdCDC: CDC Class Version 1.10
	0x01,
	
	//! 5.2.3.2 Call Management Functional Descriptor (usbcdc11.pdf)
	5, // bFunctionLength
	CDCGenericDescriptor_INTERFACE, // bDescriptor Type: CS_INTERFACE
	CDCGenericDescriptor_CALLMANAGEMENT,       // bDescriptor Subtype: Call Management Func Desc
	0x00,             // bmCapabilities: D1 + D0
	0x01,             // bDataInterface: Data Class Interface 1
	
	//! 5.2.3.3 Abstract Control Management Functional Descriptor (usbcdc11.pdf)
	4,             // bFunctionLength
	CDCGenericDescriptor_INTERFACE, // bDescriptor Type: CS_INTERFACE
	CDCGenericDescriptor_ABSTRACTCONTROLMANAGEMENT,       // bDescriptor Subtype: ACM Func Desc
	0x00,             // bmCapabilities
	
	//! 5.2.3.8 Union Functional Descriptor (usbcdc11.pdf)
	5,             // bFunctionLength
	CDCGenericDescriptor_INTERFACE, // bDescriptorType: CS_INTERFACE
	CDCGenericDescriptor_UNION,     // bDescriptor Subtype: Union Func Desc
	0, // Number of master interface is #0
	1, // First slave interface is #1
	
	//! Endpoint 1 descriptor
	//! Table 9-13. Standard Endpoint Descriptor
	7,                    // bLength
	USBGenericDescriptor_ENDPOINT, // bDescriptorType
	0x80 | EP_INTER,     // bEndpointAddress, Endpoint EP_INTER - IN
	USBEndpointDescriptor_INTERRUPT, // bmAttributes      INT
	0x40, 0x00,              // wMaxPacketSize
	0x10,                    // bInterval, for HS value between 0x01 and 0x10 = 16ms here
	
	//! Table 9-12. Standard Interface Descriptor
	9,                     // bLength
	USBGenericDescriptor_INTERFACE,// bDescriptorType
	1, // This is interface #1
	0, // This is alternate setting #0 for this interface
	2, // This interface uses 2 endpoints
	CDCDataInterfaceDescriptor_CLASS,
	CDCDataInterfaceDescriptor_SUBCLASS,
	CDCDataInterfaceDescriptor_NOPROTOCOL,
	0,  // No string descriptor for this interface
	
	//! First alternate setting
	//! Table 9-13. Standard Endpoint Descriptor
	7,                     // bLength
	USBGenericDescriptor_ENDPOINT,  // bDescriptorType
	EP_OUT,         // bEndpointAddress, Endpoint EP_OUT - OUT
	USBEndpointDescriptor_BULK,// bmAttributes      BULK
	0x40, 0x00,                // wMaxPacketSize = 64
	0, // Must be 0 for full-speed bulk endpoints
	
	//! Table 9-13. Standard Endpoint Descriptor
	7,                     // bLength
	USBGenericDescriptor_ENDPOINT,  // bDescriptorType
	0x80 | EP_IN,          // bEndpointAddress, Endpoint EP_IN - IN
	USBEndpointDescriptor_BULK,// bmAttributes      BULK
	0x40, 0x00,                // wMaxPacketSize = 64
	0 // Must be 0 for full-speed bulk endpoints
};

//! CDC line coding
struct cdc_line_coding {
	u32 dwDTERRate;   // Baudrate
	u8  bCharFormat;  // Stop bit
	u8  bParityType;  // Parity
	u8  bDataBits;    // data bits
};

union usb_request {
	u32 data32[2];
	u16 data16[4];
	u8 data8[8];
	struct {
		u8 bmRequestType;        //!< Characteristics of the request
		u8 bRequest;             //!< Specific request
		u16 wValue;              //!< field that varies according to request
		u16 wIndex;              //!< field that varies according to request
		u16 wLength;             //!< Number of bytes to transfer if Data
	} request;
};

static struct cdc_line_coding line = { 115200, 0, 0, 8 };

#define MAXPACKETCTRL (u16)(devDescriptor[7])
#define MAXPACKETSIZEOUT (u16)(sConfiguration[57]+(sConfiguration[58]<<8))
#define OSCMAXPACKETSIZEOUT (u16)(sOtherSpeedConfiguration[57]+(sOtherSpeedConfiguration[58]<<8))
#define MAXPACKETSIZEIN (u16)(sConfiguration[64]+(sConfiguration[65]<<8))
#define OSCMAXPACKETSIZEIN (u16)(sOtherSpeedConfiguration[64]+(sOtherSpeedConfiguration[65]<<8))
#define MAXPACKETSIZEINTER (u16)(sConfiguration[41]+(sConfiguration[42]<<8))
#define OSCMAXPACKETSIZEINTER (u16)(sOtherSpeedConfiguration[41]+(sOtherSpeedConfiguration[42]<<8))

#define BUFF_SIZE PAGE_SIZE

static u8 buff[BUFF_SIZE];
static u16 buff_start;
static u16 buff_end;

struct lan966x_uart_port {
	struct uart_port	uart;
	void __iomem		*interface_ept;
	void __iomem		*udphs;
	void __iomem		*cpu;
	u8			current_configuration;
	u16			dev_status;
	u16			ept_status;
	struct clk_bulk_data	*clks;
	int			num_clocks;
};

static inline struct lan966x_uart_port *
to_lan966x_uart_port(struct uart_port *uart)
{
	return container_of(uart, struct lan966x_uart_port, uart);
}

static struct lan966x_uart_port lan966x_ports[LAN966X_MAX_UART];

static const struct of_device_id lan966x_serial_dt_ids[] = {
	{ .compatible = "microchip,lan966x-serial" },
	{ /* sentinel */ }
};

static void lan966x_send_data(struct lan966x_uart_port *lan966x_port,
			      const u8 *data, u32 length)
{
	u32 index = 0;
	u32 cpt = 0;
	u8 *fifo;

	while (0 != (LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_TXRDY_EPTSTA0_M))
		   ;

	if(0 != length) {
		while (length) {
			cpt = min(length, (u32)MAXPACKETCTRL);
			length -= cpt;
			fifo = (u8*)lan966x_port->interface_ept;

			while (cpt--) {
				fifo[index] = data[index];
				index++;
			}

			LAN_WR(UDPHS_EPTSETSTA0_TXRDY_EPTSETSTA0(1),
			       UDPHS_EPTSETSTA0);

			while ((0 != (LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_TXRDY_EPTSTA0_M)) &&
			       ((LAN_RD(UDPHS_INTSTA) & UDPHS_INTSTA_DET_SUSPD_INTSTA_M) != UDPHS_INTSTA_DET_SUSPD_INTSTA_M))
				;

			if ((LAN_RD(UDPHS_INTSTA) & UDPHS_INTSTA_DET_SUSPD_INTSTA_M) == UDPHS_INTSTA_DET_SUSPD_INTSTA_M)
				break;
		}
	}
	else {
		while (0 != (LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_TXRDY_EPTSTA0_M))
			;

		LAN_WR(UDPHS_EPTSETSTA0_TXRDY_EPTSETSTA0(1),
		       UDPHS_EPTSETSTA0);

		while ((0 != (LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_TXRDY_EPTSTA0_M)) &&
		       ((LAN_RD(UDPHS_INTSTA) & UDPHS_INTSTA_DET_SUSPD_INTSTA_M) != UDPHS_INTSTA_DET_SUSPD_INTSTA_M))
				;
	}
}

static void lan966x_send_zlp(struct lan966x_uart_port *lan966x_port)
{
	while (0 != (LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_TXRDY_EPTSTA0_M))
		;

	LAN_WR(UDPHS_EPTSETSTA0_TXRDY_EPTSETSTA0(1), UDPHS_EPTSETSTA0);

	while ((0 != (LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_TXRDY_EPTSTA0_M)) &&
	      ((LAN_RD(UDPHS_INTSTA) & UDPHS_INTSTA_DET_SUSPD_INTSTA_M) != UDPHS_INTSTA_DET_SUSPD_INTSTA_M))
		;
}

static void lan966x_send_stall(struct lan966x_uart_port *lan966x_port)
{
	LAN_WR(UDPHS_EPTSETSTA0_FRCESTALL_EPTSETSTA0(1), UDPHS_EPTSETSTA0);
}

static u8 lan966x_size_endpoint(u16 packet_size)
{
	if (packet_size == 8) {
		return UDPHS_EPTCFG_EPT_SIZE_8;
	} else if (packet_size == 16) {
		return UDPHS_EPTCFG_EPT_SIZE_16;
	} else if (packet_size == 32) {
		return UDPHS_EPTCFG_EPT_SIZE_32;
	} else if (packet_size == 64) {
		return UDPHS_EPTCFG_EPT_SIZE_64;
	} else if (packet_size == 128) {
		return UDPHS_EPTCFG_EPT_SIZE_128;
	} else if (packet_size == 256) {
		return UDPHS_EPTCFG_EPT_SIZE_256;
	} else if (packet_size == 512) {
		return UDPHS_EPTCFG_EPT_SIZE_512;
	} else if (packet_size == 1024) {
		return UDPHS_EPTCFG_EPT_SIZE_1024;
	}

	return 0;
}

static void lan966x_enumerate(struct uart_port *port,
			      struct lan966x_uart_port *lan966x_port)
{
	u32 *interface_ept = lan966x_port->interface_ept;
	union usb_request setup_data;
	u16 wMaxPacketSizeINTER;
	u16 wMaxPacketSizeOUT;
	u16 wMaxPacketSizeIN;
	u16 sizeEpt;

	if ((LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_RX_SETUP_EPTSTA0_M) != UDPHS_EPTSTA0_RX_SETUP_EPTSTA0_M)
		return;

	setup_data.data32[0] = readl(interface_ept);
	setup_data.data32[1] = readl(interface_ept);

	LAN_WR(UDPHS_EPTCLRSTA0_RX_SETUP_EPTCLRSTA0(1), UDPHS_EPTCLRSTA0);

	// Handle supported standard device request Cf Table 9-3 in USB specification Rev 1.1
	switch (setup_data.request.bRequest) {
	case USBGenericRequest_GETDESCRIPTOR:
		if (setup_data.request.wValue == (USBGenericDescriptor_DEVICE << 8)) {
			lan966x_send_data(lan966x_port, devDescriptor, min(sizeof(devDescriptor), (u32)setup_data.request.wLength));

			// Waiting for Status stage
			while ((LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_RXRDY_TXKL_EPTSTA0_M) != UDPHS_EPTSTA0_RXRDY_TXKL_EPTSTA0_M)
				;

			LAN_WR(UDPHS_EPTCLRSTA0_RXRDY_TXKL_EPTCLRSTA0(1),
			       UDPHS_EPTCLRSTA0);
		}
		else if (setup_data.request.wValue == (USBGenericDescriptor_CONFIGURATION << 8)) {
			if (LAN_RD(UDPHS_INTSTA) & UDPHS_INTSTA_SPEED_M) {
				sConfiguration[1] = USBGenericDescriptor_CONFIGURATION;
				lan966x_send_data(lan966x_port, sConfiguration, min(sizeof(sConfiguration), (u32)setup_data.request.wLength));

				// Waiting for Status stage
				while ((LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_RXRDY_TXKL_EPTSTA0_M) != UDPHS_EPTSTA0_RXRDY_TXKL_EPTSTA0_M)
					;

				LAN_WR(UDPHS_EPTCLRSTA0_RXRDY_TXKL_EPTCLRSTA0(1),
				       UDPHS_EPTCLRSTA0);
			}
			else {
				sOtherSpeedConfiguration[1] = USBGenericDescriptor_CONFIGURATION;
				lan966x_send_data(lan966x_port, sOtherSpeedConfiguration, min(sizeof(sOtherSpeedConfiguration), (u32)setup_data.request.wLength));

				// Waiting for Status stage
				while ((LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_RXRDY_TXKL_EPTSTA0_M) != UDPHS_EPTSTA0_RXRDY_TXKL_EPTSTA0_M)
					;

				LAN_WR(UDPHS_EPTCLRSTA0_RXRDY_TXKL_EPTCLRSTA0(1),
				       UDPHS_EPTCLRSTA0);
			}
		}
		else {
			lan966x_send_stall(lan966x_port);
		}
		break;

	case USBGenericRequest_SETADDRESS:
		lan966x_send_zlp(lan966x_port);
		LAN_RMW(UDPHS_CTRL_DEV_ADDR(setup_data.request.wValue & 0x7F) |
			UDPHS_CTRL_FADDR_EN(1),
			UDPHS_CTRL_DEV_ADDR_M |
			UDPHS_CTRL_FADDR_EN_M,
			UDPHS_CTRL);
		break;

	case USBGenericRequest_SETCONFIGURATION:
		lan966x_port->current_configuration = (uint8_t)setup_data.request.wValue;  // The lower byte of the wValue field specifies the desired configuration.
		lan966x_send_zlp(lan966x_port);

		if (LAN_RD(UDPHS_INTSTA) & UDPHS_INTSTA_SPEED_M) {
			// High Speed
			wMaxPacketSizeOUT   = MAXPACKETSIZEOUT;
			wMaxPacketSizeIN    = MAXPACKETSIZEIN;
			wMaxPacketSizeINTER = MAXPACKETSIZEINTER;
		} else {
			// Full Speed
			wMaxPacketSizeOUT   = OSCMAXPACKETSIZEOUT;
			wMaxPacketSizeIN    = OSCMAXPACKETSIZEIN;
			wMaxPacketSizeINTER = OSCMAXPACKETSIZEINTER;
		}

		sizeEpt = lan966x_size_endpoint(wMaxPacketSizeOUT);
		LAN_WR(UDPHS_EPTCFG1_EPT_SIZE_EPTCFG1(sizeEpt) |
		       UDPHS_EPTCFG1_EPT_TYPE_EPTCFG1(2) |
		       UDPHS_EPTCFG1_BK_NUMBER_EPTCFG1(2),
		       UDPHS_EPTCFG1);
		while ((LAN_RD(UDPHS_EPTCFG1) & UDPHS_EPTCFG1_EPT_MAPD_EPTCFG1_M) != UDPHS_EPTCFG1_EPT_MAPD_EPTCFG1_M)
			;
		LAN_WR(UDPHS_EPTCTLENB1_RXRDY_TXKL_EPTCTLENB1(1) |
		       UDPHS_EPTCTLENB1_EPT_ENABL_EPTCTLENB1(1),
		       UDPHS_EPTCTLENB1);

		sizeEpt = lan966x_size_endpoint(wMaxPacketSizeIN);
		LAN_WR(UDPHS_EPTCFG2_EPT_SIZE_EPTCFG2(sizeEpt) |
		       UDPHS_EPTCFG2_EPT_DIR_EPTCFG2(1) |
		       UDPHS_EPTCFG2_EPT_TYPE_EPTCFG2(2) |
		       UDPHS_EPTCFG2_BK_NUMBER_EPTCFG2(2),
		       UDPHS_EPTCFG2);
		while ((LAN_RD(UDPHS_EPTCFG2) & UDPHS_EPTCFG2_EPT_MAPD_EPTCFG2_M) != UDPHS_EPTCFG2_EPT_MAPD_EPTCFG2_M)
			;
		LAN_WR(UDPHS_EPTCTLENB2_SHRT_PCKT_EPTCTLENB2(1) |
		       UDPHS_EPTCTLENB2_EPT_ENABL_EPTCTLENB2(1),
		       UDPHS_EPTCTLENB2);

		sizeEpt = lan966x_size_endpoint(wMaxPacketSizeINTER);
		LAN_WR(UDPHS_EPTCFG3_EPT_SIZE_EPTCFG3(sizeEpt) |
		       UDPHS_EPTCFG3_EPT_DIR_EPTCFG3(1) |
		       UDPHS_EPTCFG3_EPT_TYPE_EPTCFG3(3) |
		       UDPHS_EPTCFG3_BK_NUMBER_EPTCFG3(1),
		       UDPHS_EPTCFG3);
		while ((LAN_RD(UDPHS_EPTCFG3) & UDPHS_EPTCFG3_EPT_MAPD_EPTCFG3_M) != UDPHS_EPTCFG3_EPT_MAPD_EPTCFG3_M)
			;
		LAN_WR(UDPHS_EPTCTLENB3_EPT_ENABL_EPTCTLENB3(1),
		       UDPHS_EPTCTLENB3);

		break;

	case USBGenericRequest_GETCONFIGURATION:
		lan966x_send_data(lan966x_port, (u8*) &(lan966x_port->current_configuration), sizeof(lan966x_port->current_configuration));
		break;

	// handle CDC class requests
	case CDCGenericRequest_SETLINECODING:
		// Waiting for Status stage
		while ((LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_RXRDY_TXKL_EPTSTA0_M) != UDPHS_EPTSTA0_RXRDY_TXKL_EPTSTA0_M)
			;

		LAN_WR(UDPHS_EPTCLRSTA0_RXRDY_TXKL_EPTCLRSTA0(1),
		       UDPHS_EPTCLRSTA0);

		lan966x_send_zlp(lan966x_port);
                break;

	case CDCGenericRequest_GETLINECODING:
		lan966x_send_data(lan966x_port, (u8*) &line, min(sizeof(line), (u32)setup_data.request.wLength));
		break;

	case CDCGenericRequest_SETCONTROLLINESTATE:
		lan966x_send_zlp(lan966x_port);
		break;

	case CDCGenericRequest_BREAK:
		if (!port->sysrq)
			uart_handle_break(port);
		lan966x_send_zlp(lan966x_port);
		break;

	// case USBGenericRequest_SETINTERFACE:  MUST BE STALL for us
	default:
		lan966x_send_stall(lan966x_port);
		break;
	}
}

static u8 lan966x_is_configured(struct lan966x_uart_port *lan966x_port)
{
	u32 isr = LAN_RD(UDPHS_INTSTA);
	u16 size_ept;

	// Resume
	if ((isr & UDPHS_INTSTA_WAKE_UP_INTSTA_M) ||
	    (isr & UDPHS_INTSTA_ENDOFRSM_INTSTA_M))
		LAN_RMW(UDPHS_CLRINT_WAKE_UP_CLRINT(1) |
			UDPHS_CLRINT_ENDOFRSM_CLRINT(1),
			UDPHS_CLRINT_WAKE_UP_CLRINT_M |
			UDPHS_CLRINT_ENDOFRSM_CLRINT_M,
			UDPHS_CLRINT);

	if (isr & (UDPHS_INTSTA_INT_SOF_INTSTA_M)) {
		LAN_RMW(UDPHS_CLRINT_INT_SOF_CLRINT(1),
			UDPHS_CLRINT_INT_SOF_CLRINT_M,
			UDPHS_CLRINT);
	} else {
		if (isr & (UDPHS_INTSTA_MICRO_SOF_INTSTA_M)) {
			LAN_RMW(UDPHS_CLRINT_MICRO_SOF_CLRINT(1),
				UDPHS_CLRINT_MICRO_SOF_CLRINT_M,
				UDPHS_CLRINT);
		}
	}

	// Suspend
	if (isr & UDPHS_INTSTA_DET_SUSPD_INTSTA_M) {
		lan966x_port->current_configuration = 0;
		LAN_RMW(UDPHS_CLRINT_DET_SUSPD_CLRINT(1),
			UDPHS_CLRINT_DET_SUSPD_CLRINT_M,
			UDPHS_CLRINT);
	} else {
		if (isr & UDPHS_INTSTA_ENDRESET_INTSTA_M) {
			lan966x_port->current_configuration = 0;

			size_ept = lan966x_size_endpoint(MAXPACKETCTRL);

			LAN_RMW(UDPHS_EPTCFG0_EPT_SIZE_EPTCFG0(size_ept) |
				UDPHS_EPTCFG0_EPT_TYPE_EPTCFG0(0) |
				UDPHS_EPTCFG0_BK_NUMBER_EPTCFG0(1),
				UDPHS_EPTCFG0_EPT_SIZE_EPTCFG0_M |
				UDPHS_EPTCFG0_EPT_TYPE_EPTCFG0_M |
				UDPHS_EPTCFG0_BK_NUMBER_EPTCFG0_M,
				UDPHS_EPTCFG0);

			while( (LAN_RD(UDPHS_EPTCFG0) & UDPHS_EPTCFG0_EPT_MAPD_EPTCFG0_M) != UDPHS_EPTCFG0_EPT_MAPD_EPTCFG0_M)
				;

			LAN_RMW(UDPHS_IEN_EPT_X(GENMASK(16,0)) |
				UDPHS_IEN_ENDRESET(1) |
				UDPHS_IEN_DET_SUSPD(1),
				UDPHS_IEN_EPT_X_M |
				UDPHS_IEN_ENDRESET_M |
				UDPHS_IEN_DET_SUSPD_M,
				UDPHS_IEN);

			LAN_RMW(UDPHS_EPTCTLENB0_RX_SETUP_EPTCTLENB0(1) |
				UDPHS_EPTCTLENB0_EPT_ENABL_EPTCTLENB0(1),
				UDPHS_EPTCTLENB0_RX_SETUP_EPTCTLENB0_M |
				UDPHS_EPTCTLENB0_EPT_ENABL_EPTCTLENB0_M,
				UDPHS_EPTCTLENB0);

			LAN_RMW(UDPHS_CLRINT_ENDRESET_CLRINT(1),
				UDPHS_CLRINT_ENDRESET_CLRINT_M,
				UDPHS_CLRINT);

		}
	}

	return lan966x_port->current_configuration;
}

static u32 lan966x_usb_write(struct lan966x_uart_port *lan966x_port,
			     const u8 *data, u32 length)
{
	u32 packet_size = 512;
	u32 cpt = 0;
	u8 *fifo;

	while (0 != (LAN_RD(UDPHS_EPTSTA2) & UDPHS_EPTSTA2_TXRDY_EPTSTA2_M))
		;

	while (length) {
		cpt = min(length, packet_size);
		length -= cpt;
		fifo = (u8*)((u32*)lan966x_port->interface_ept + (16834 * EP_IN));

		while (cpt) {
			//writeb('f', lan966x_port->interface_ept + (16834 * EP_IN));
			*(fifo++) = *(data++);
			--cpt;
		}

		LAN_WR(UDPHS_EPTSETSTA2_TXRDY_EPTSETSTA2(1),
		       UDPHS_EPTSETSTA2);

		while (((LAN_RD(UDPHS_EPTSTA2) & UDPHS_EPTSTA2_TXRDY_EPTSTA2_M)) &&
		       ((LAN_RD(UDPHS_INTSTA) & UDPHS_INTSTA_DET_SUSPD_INTSTA_M) != UDPHS_INTSTA_DET_SUSPD_INTSTA_M))
				;

		if ((LAN_RD(UDPHS_INTSTA) & UDPHS_INTSTA_DET_SUSPD_INTSTA_M) == UDPHS_INTSTA_DET_SUSPD_INTSTA_M)
			break;
	}

	return length;
}

static u32 lan966x_usb_read(struct lan966x_uart_port *lan966x_port, u8 *data)
{
	u32 status;
	u32 recv;
	u8 *fifo;
	u32 size;

	while ((status = LAN_RD(UDPHS_EPTSTA1)) &
	       UDPHS_EPTSTA1_RXRDY_TXKL_EPTSTA1_M) {

		recv = 0;

		size = (LAN_RD(UDPHS_EPTSTA1) & UDPHS_EPTSTA1_BYTE_COUNT_EPTSTA1_M) >> 20;
		fifo = (u8*)((u32*)lan966x_port->interface_ept + (16834 * EP_OUT));

		while (size--) {
			/* Maybe it should be readb */
			data[buff_end] = fifo[recv];
			recv++;

			buff_end = (buff_end + 1) % BUFF_SIZE;
			if (buff_end == buff_start)
				break;
		}

		LAN_WR(UDPHS_EPTCLRSTA1_RXRDY_TXKL_EPTCLRSTA1(1),
		       UDPHS_EPTCLRSTA1);
	}

	return 0;
}

static unsigned int lan966x_tx_empty(struct uart_port *port)
{
	struct lan966x_uart_port *lan966x_port = to_lan966x_uart_port(port);

	return LAN_RD(UDPHS_EPTSTA2) & UDPHS_EPTSTA2_TXRDY_EPTSTA2_M;
}

static unsigned int lan966x_get_mctrl(struct uart_port *port)
{
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void lan966x_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static void lan966x_stop_tx(struct uart_port *port)
{
}

static void lan966x_start_tx(struct uart_port *port)
{
	struct lan966x_uart_port *lan966x_port = to_lan966x_uart_port(port);
	struct circ_buf *xmit = &port->state->xmit;
	unsigned char ch;

	while (!uart_circ_empty(xmit)) {
		ch = xmit->buf[xmit->tail];
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		lan966x_usb_write(lan966x_port, (u8*)&ch, 1);
	}
}

static void lan966x_stop_rx(struct uart_port *port)
{
}

static void lan966x_break_ctl(struct uart_port *port, int break_state)
{
}

static irqreturn_t lan966x_data_out(struct uart_port *port,
				    struct lan966x_uart_port *lan966x_port)
{
	unsigned int flg = 0;
	unsigned int status;
	unsigned int ch;

	flg = TTY_NORMAL;

	spin_lock(&port->lock);
	while ((status = LAN_RD(UDPHS_EPTSTA1)) &
	       UDPHS_EPTSTA1_RXRDY_TXKL_EPTSTA1_M) {

		lan966x_usb_read(lan966x_port, buff);
		while (buff_start != buff_end) {

			ch = buff[buff_start];
			buff_start = (buff_start + 1) % BUFF_SIZE;

			port->icount.rx++;

			if (!(uart_handle_sysrq_char(port, ch)))
				uart_insert_char(port, status,
						 UDPHS_EPTSTA1_ERR_OVFLW_EPTSTA1_M,
						 ch, flg);
		}
	}

	/*
	 * Drop the lock here since it might end up calling
	 * uart_start(), which takes the lock.
	 */
	spin_unlock(&port->lock);
	tty_flip_buffer_push(&port->state->port);

	return IRQ_HANDLED;
}

static irqreturn_t lan966x_isr(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;
	struct lan966x_uart_port *lan966x_port = to_lan966x_uart_port(port);
	unsigned int status;

	status = LAN_RD(UDPHS_INTSTA);
	if ((status & UDPHS_INTSTA_DET_SUSPD_INTSTA_M) ||
	    (status & UDPHS_INTSTA_ENDRESET_INTSTA_M))
		lan966x_is_configured(lan966x_port);

	if (LAN_RD(UDPHS_EPTSTA1) & UDPHS_EPTSTA1_RXRDY_TXKL_EPTSTA1_M)
		return lan966x_data_out(port, lan966x_port);

	if (LAN_RD(UDPHS_EPTSTA0) & UDPHS_EPTSTA0_RX_SETUP_EPTSTA0_M)
		lan966x_enumerate(port, lan966x_port);

	return IRQ_HANDLED;
}

static void lan966x_enable_irq(struct uart_port *port)
{
	struct lan966x_uart_port *lan966x_port = to_lan966x_uart_port(port);

	LAN_RMW(UDPHS_EPTCTLENB0_RX_SETUP_EPTCTLENB0(1),
		UDPHS_EPTCTLENB0_RX_SETUP_EPTCTLENB0_M,
		UDPHS_EPTCTLENB0);

	/* Enable interrupt for out direction(host -> device)*/
	LAN_RMW(UDPHS_EPTCTLENB1_RXRDY_TXKL_EPTCTLENB1(1),
		UDPHS_EPTCTLENB1_RXRDY_TXKL_EPTCTLENB1_M,
		UDPHS_EPTCTLENB1);

	/* Enable EPT, Reset and SUSPD interrupts */
	LAN_RMW(UDPHS_IEN_EPT_X(GENMASK(16,0)) |
		UDPHS_IEN_ENDRESET(1) |
		UDPHS_IEN_DET_SUSPD(1),
		UDPHS_IEN_EPT_X_M |
		UDPHS_IEN_ENDRESET_M |
		UDPHS_IEN_DET_SUSPD_M,
		UDPHS_IEN);
}

static void lan966x_disable_irq(struct uart_port *port)
{
	struct lan966x_uart_port *lan966x_port = to_lan966x_uart_port(port);

	/* Disable all EPT, Reset and SUSPDinterrupts */
	LAN_RMW(UDPHS_IEN_EPT_X(0) |
		UDPHS_IEN_ENDRESET(0) |
		UDPHS_IEN_DET_SUSPD(0),
		UDPHS_IEN_EPT_X_M |
		UDPHS_IEN_ENDRESET_M |
		UDPHS_IEN_DET_SUSPD_M,
		UDPHS_IEN);
}

static int lan966x_startup(struct uart_port *port)
{
	lan966x_disable_irq(port);

	if (request_irq(port->irq, lan966x_isr, 0, "lan966x uart", port)) {
		dev_warn(port->dev, "Unable to attach Lan966x UART intr\n");
		return -EBUSY;
	}

	lan966x_enable_irq(port);

	return 0;
}

static void lan966x_shutdown(struct uart_port *port)
{
}

static void lan966x_set_termios(struct uart_port *port,
				struct ktermios *new, struct ktermios *old)
{
	unsigned int baud;

	new->c_cflag &= ~(CMSPAR|CRTSCTS|CSIZE);
	new->c_cflag |= CS8;
	baud = uart_get_baud_rate(port, new, old, 0, 115200);

	uart_update_timeout(port, new->c_cflag, baud);
}

static const char *lan966x_type(struct uart_port *port)
{
	return port->type == PORT_LAN966X ? "LAN966X_SERIAL" : NULL;
}

static void lan966x_release_port(struct uart_port *port)
{
}

static int lan966x_request_port(struct uart_port *port)
{
	return 0;
}

static void lan966x_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_LAN966X;
}

static int lan966x_verify_port(struct uart_port *port,
			       struct serial_struct *ser)
{
	if (port->type != PORT_UNKNOWN && ser->type != PORT_LAN966X)
		return -EINVAL;

	return 0;
}

static const struct uart_ops lan966x_ops = {
	.tx_empty = lan966x_tx_empty,
	.set_mctrl = lan966x_set_mctrl,
	.get_mctrl = lan966x_get_mctrl,
	.stop_tx = lan966x_stop_tx,
	.start_tx = lan966x_start_tx,
	.stop_rx = lan966x_stop_rx,
	.break_ctl = lan966x_break_ctl,
	.startup = lan966x_startup,
	.shutdown = lan966x_shutdown,
	.set_termios = lan966x_set_termios,
	.type = lan966x_type,
	.release_port = lan966x_release_port,
	.request_port = lan966x_request_port,
	.config_port = lan966x_config_port,
	.verify_port = lan966x_verify_port,
};

#ifdef CONFIG_SERIAL_LAN966X_CONSOLE
static void lan966x_console_putchar(struct uart_port *port, int ch)
{
	struct lan966x_uart_port *lan966x_port = to_lan966x_uart_port(port);

	while (0 != (LAN_RD(UDPHS_EPTSTA2) & UDPHS_EPTSTA2_TXRDY_EPTSTA2_M)) {
		cpu_relax();
	}

	lan966x_usb_write(lan966x_port, (u8*)&ch, 1);
}

static void lan966x_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *port = &lan966x_ports[0].uart;

	uart_console_write(port, s, count, lan966x_console_putchar);
}

static int __init lan966x_console_setup(struct console *co, char *options)
{
	struct uart_port *port = &lan966x_ports[0].uart;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct uart_driver lan966x_uart;

static struct console lan966x_console = {
	.name		= LAN966X_SERIAL_DEVNAME,
	.write		= lan966x_console_write,
	.device		= uart_console_device,
	.setup		= lan966x_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &lan966x_uart,
};
#endif

static struct uart_driver lan966x_uart = {
	.owner		= THIS_MODULE,
	.driver_name	= "lan966x_serial",
	.dev_name	= LAN966X_SERIAL_DEVNAME,
	.major		= LAN966X_SERIAL_MAJOR,
	.minor		= LAN966X_SERIAL_MINOR,
	.nr		= LAN966X_MAX_UART,
#ifdef CONFIG_SERIAL_LAN966X_CONSOLE
	.cons		= &lan966x_console,
#endif
};

static int lan966x_serial_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct lan966x_uart_port *lan966x_port;
	struct resource *resource;
	int ret;

	if (!np)
		return -ENODEV;

	ret = of_alias_get_id(np, "serial");
	if (ret < 0)
		ret = 0;

	lan966x_port = &lan966x_ports[0];
	lan966x_port->uart.dev = &pdev->dev;
	lan966x_port->uart.iotype = UPIO_MEM;
	lan966x_port->uart.flags = UPF_BOOT_AUTOCONF | UPF_IOREMAP;
	lan966x_port->uart.line = ret;
	lan966x_port->uart.ops = &lan966x_ops;
	lan966x_port->uart.has_sysrq = IS_ENABLED(CONFIG_SERIAL_LAN966X_CONSOLE);
	lan966x_port->uart.fifosize = 1;
	lan966x_port->uart.irq = irq_of_parse_and_map(np, 0);

	/* Main access to USB Device */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"udphs");
	if (!resource)
		return -ENODEV;
	lan966x_port->udphs = ioremap(resource->start,
				      resource_size(resource));
	lan966x_port->uart.mapbase = (unsigned long)lan966x_port->udphs;

	/* We need also this */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"interface_ept");
	if (!resource)
		return -ENODEV;
	lan966x_port->interface_ept = ioremap(resource->start,
					      resource_size(resource));

	ret = clk_bulk_get_all(lan966x_port->uart.dev, &lan966x_port->clks);
	if (ret < 0)
		goto err_clock;

	lan966x_port->num_clocks = ret;
	ret = clk_bulk_prepare_enable(lan966x_port->num_clocks,
				      lan966x_port->clks);
	if (ret)
		goto err_clock;

	return uart_add_one_port(&lan966x_uart, &lan966x_port->uart);

err_clock:
	return -ENODEV;
}

static int lan966x_serial_remove(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);
	int ret = 0;

	device_init_wakeup(&pdev->dev, 0);

	ret = uart_remove_one_port(&lan966x_uart, port);

	port->line = 0;

	pdev->dev.of_node = NULL;
	return ret;
}

static struct platform_driver lan966x_serial_driver = {
	.probe		= lan966x_serial_probe,
	.remove		= lan966x_serial_remove,
	.driver		= {
		.name			= "lan966x_usart_serial",
		.of_match_table		= of_match_ptr(lan966x_serial_dt_ids),
	},
};

static int __init lan966x_serial_init(void)
{
	int ret;

	ret = uart_register_driver(&lan966x_uart);
	if (ret)
		return ret;

	ret = platform_driver_register(&lan966x_serial_driver);
	if (ret)
		uart_unregister_driver(&lan966x_uart);

	return ret;
}
late_initcall(lan966x_serial_init);
