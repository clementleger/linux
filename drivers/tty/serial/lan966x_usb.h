/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2011, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

#ifndef _USB_H
#define _USB_H


//------------------------------------------------------------------------------
/// \page "USB Request codes"
///
/// This page lists the USB generic request codes.
///
/// !Codes
/// - USBGenericRequest_GETSTATUS
/// - USBGenericRequest_CLEARFEATURE
/// - USBGenericRequest_SETFEATURE
/// - USBGenericRequest_SETADDRESS
/// - USBGenericRequest_GETDESCRIPTOR
/// - USBGenericRequest_SETDESCRIPTOR
/// - USBGenericRequest_GETCONFIGURATION
/// - USBGenericRequest_SETCONFIGURATION
/// - USBGenericRequest_GETINTERFACE
/// - USBGenericRequest_SETINTERFACE
/// - USBGenericRequest_SYNCHFRAME

/// GET_STATUS request code.
#define USBGenericRequest_GETSTATUS             0
/// CLEAR_FEATURE request code.
#define USBGenericRequest_CLEARFEATURE          1
/// SET_FEATURE request code.
#define USBGenericRequest_SETFEATURE            3
/// SET_ADDRESS request code.
#define USBGenericRequest_SETADDRESS            5
/// GET_DESCRIPTOR request code.
#define USBGenericRequest_GETDESCRIPTOR         6
/// SET_DESCRIPTOR request code.
#define USBGenericRequest_SETDESCRIPTOR         7
/// GET_CONFIGURATION request code.
#define USBGenericRequest_GETCONFIGURATION      8
/// SET_CONFIGURATION request code.
#define USBGenericRequest_SETCONFIGURATION      9
/// GET_INTERFACE request code.
#define USBGenericRequest_GETINTERFACE          10
/// SET_INTERFACE request code.
#define USBGenericRequest_SETINTERFACE          11
/// SYNCH_FRAME request code.
#define USBGenericRequest_SYNCHFRAME            12

//------------------------------------------------------------------------------
/// \page "USB Descriptor types"
///
/// This page lists the codes of the usb descriptor types
///
/// !Types
/// - USBGenericDescriptor_DEVICE
/// - USBGenericDescriptor_CONFIGURATION
/// - USBGenericDescriptor_STRING
/// - USBGenericDescriptor_INTERFACE
/// - USBGenericDescriptor_ENDPOINT
/// - USBGenericDescriptor_DEVICEQUALIFIER
/// - USBGenericDescriptor_OTHERSPEEDCONFIGURATION
/// - USBGenericDescriptor_INTERFACEPOWER
/// - USBGenericDescriptor_OTG
/// - USBGenericDescriptor_DEBUG
/// - USBGenericDescriptor_INTERFACEASSOCIATION

/// Device descriptor type.
#define USBGenericDescriptor_DEVICE                     1
/// Configuration descriptor type.
#define USBGenericDescriptor_CONFIGURATION              2
/// String descriptor type.
#define USBGenericDescriptor_STRING                     3
/// Interface descriptor type.
#define USBGenericDescriptor_INTERFACE                  4
/// Endpoint descriptor type.
#define USBGenericDescriptor_ENDPOINT                   5
/// Device qualifier descriptor type.
#define USBGenericDescriptor_DEVICEQUALIFIER            6
/// Other speed configuration descriptor type.
#define USBGenericDescriptor_OTHERSPEEDCONFIGURATION    7
/// Interface power descriptor type.
#define USBGenericDescriptor_INTERFACEPOWER             8
/// On-The-Go descriptor type.
#define USBGenericDescriptor_OTG                        9
/// Debug descriptor type.
#define USBGenericDescriptor_DEBUG                      10
/// Interface association descriptor type.
#define USBGenericDescriptor_INTERFACEASSOCIATION       11

//------------------------------------------------------------------------------
/// \page "USB Test mode selectors"
///
/// This page lists codes of USB high speed test mode selectors.
///
/// !Selectors
/// - USBFeatureRequest_TESTJ
/// - USBFeatureRequest_TESTK
/// - USBFeatureRequest_TESTSE0NAK
/// - USBFeatureRequest_TESTPACKET
/// - USBFeatureRequest_TESTFORCEENABLE
/// - USBFeatureRequest_TESTSENDZLP

/// Tests the high-output drive level on the D+ line.
#define USBFeatureRequest_TESTJ                 1
/// Tests the high-output drive level on the D- line.
#define USBFeatureRequest_TESTK                 2
/// Tests the output impedance, low-level output voltage and loading
/// characteristics.
#define USBFeatureRequest_TESTSE0NAK            3
/// Tests rise and fall times, eye patterns and jitter.
#define USBFeatureRequest_TESTPACKET            4
/// Tests the hub disconnect detection.
#define USBFeatureRequest_TESTFORCEENABLE       5


//------------------------------------------------------------------------------
/// \page "USB Request Recipients"
///
/// This page lists codes of USB request recipients.
///
/// !Recipients
/// - USBGenericRequest_DEVICE
/// - USBGenericRequest_INTERFACE
/// - USBGenericRequest_ENDPOINT
/// - USBGenericRequest_OTHER

/// Recipient is the whole device.
#define USBGenericRequest_DEVICE                0
/// Recipient is an interface.
#define USBGenericRequest_INTERFACE             1
/// Recipient is an endpoint.
#define USBGenericRequest_ENDPOINT              2
/// Recipient is another entity.
#define USBGenericRequest_OTHER                 3

//------------------------------------------------------------------------------
/// \page "USB Feature selectors"
///
/// This page lists codes of USB feature selectors.
///
/// !Selectors
/// - USBFeatureRequest_ENDPOINTHALT
/// - USBFeatureRequest_DEVICEREMOTEWAKEUP
/// - USBFeatureRequest_TESTMODE

/// Halt feature of an endpoint.
#define USBFeatureRequest_ENDPOINTHALT          0
/// Remote wake-up feature of the device.
#define USBFeatureRequest_DEVICEREMOTEWAKEUP    1
/// Test mode of the device.
#define USBFeatureRequest_TESTMODE              2
/// OTG set feature
#define USBFeatureRequest_OTG                0x0B

/// On The Go Feature Selectors
/// b_hnp_enable      3
/// a_hnp_support     4
/// a_alt_hnp_support 5
#define USBFeatureRequest_OTG_B_HNP_ENABLE      3
#define USBFeatureRequest_OTG_A_HNP_SUPPORT     4
#define USBFeatureRequest_OTG_A_ALT_HNP_SUPPORT 5

//------------------------------------------------------------------------------
/// \page "USB Endpoint types"
///
/// This page lists definitions of USB endpoint types.
///
/// !Types
/// - USBEndpointDescriptor_CONTROL
/// - USBEndpointDescriptor_ISOCHRONOUS
/// - USBEndpointDescriptor_BULK
/// - USBEndpointDescriptor_INTERRUPT

/// Control endpoint type.
#define USBEndpointDescriptor_CONTROL       0
/// Isochronous endpoint type.
#define USBEndpointDescriptor_ISOCHRONOUS   1
/// Bulk endpoint type.
#define USBEndpointDescriptor_BULK          2
/// Interrupt endpoint type.
#define USBEndpointDescriptor_INTERRUPT     3

//------------------------------------------------------------------------------
//! Definitions for bits in the bmAttributes field of
//!   (Table 9-10) Standard Configuration Descriptor
// Reserved (reset to zero)             //   D4...D0=0
#define USB_CONFIG_BUS_NOWAKEUP    0x80 //!< D5=0 + D6=0 + D7=1
#define USB_CONFIG_SELF_NOWAKEUP   0xC0 //!< D5=0 + D6=1 + D7=1
#define USB_CONFIG_BUS_WAKEUP      0xA0 //!< D5=1 + D6=0 + D7=1
#define USB_CONFIG_SELF_WAKEUP     0xE0 //!< D5=1 + D6=1 + D7=1
// Reserved (set to one)                //   D7=1


//------------------------------------------------------------------------------
/// \page "CDC Device Descriptor Values"
/// This page lists the values for CDC Device Descriptor.
///
/// !Values
/// - CDCDeviceDescriptor_CLASS
/// - CDCDeviceDescriptor_SUBCLASS
/// - CDCDeviceDescriptor_PROTOCOL

/// Device class code when using the CDC class.
#define CDCDeviceDescriptor_CLASS               0x02
/// Device subclass code when using the CDC class.
#define CDCDeviceDescriptor_SUBCLASS            0x00
/// Device protocol code when using the CDC class.
#define CDCDeviceDescriptor_PROTOCOL            0x00

//------------------------------------------------------------------------------
/// \page "CDC Communication Interface Values"
/// This page lists the values for CDC Communication Interface Descriptor.
///
/// !Values
/// - CDCCommunicationInterfaceDescriptor_CLASS
/// - CDCCommunicationInterfaceDescriptor_ABSTRACTCONTROLMODEL
/// - CDCCommunicationInterfaceDescriptor_NOPROTOCOL

/// Interface class code for a CDC communication class interface.
#define CDCCommunicationInterfaceDescriptor_CLASS                   0x02
/// Interface subclass code for an Abstract Control Model interface descriptor.
#define CDCCommunicationInterfaceDescriptor_ABSTRACTCONTROLMODEL    0x02
/// Interface protocol code when a CDC communication interface does not
/// implemenent any particular protocol.
#define CDCCommunicationInterfaceDescriptor_NOPROTOCOL              0x00

//------------------------------------------------------------------------------
/// \page "CDC Descriptro Types"
/// This page lists CDC descriptor types.
///
/// !Types
/// - CDCGenericDescriptor_INTERFACE
/// - CDCGenericDescriptor_ENDPOINT

///Indicates that a CDC descriptor applies to an interface.
#define CDCGenericDescriptor_INTERFACE                          0x24
/// Indicates that a CDC descriptor applies to an endpoint.
#define CDCGenericDescriptor_ENDPOINT                           0x25

//------------------------------------------------------------------------------
/// \page "CDC Descriptor Subtypes"
/// This page lists CDC descriptor sub types
///
/// !Types
/// - CDCGenericDescriptor_HEADER
/// - CDCGenericDescriptor_CALLMANAGEMENT
/// - CDCGenericDescriptor_ABSTRACTCONTROLMANAGEMENT
/// - CDCGenericDescriptor_UNION

/// Header functional descriptor subtype.
#define CDCGenericDescriptor_HEADER                             0x00
/// Call management functional descriptor subtype.
#define CDCGenericDescriptor_CALLMANAGEMENT                     0x01
/// Abstract control management descriptor subtype.
#define CDCGenericDescriptor_ABSTRACTCONTROLMANAGEMENT          0x02
/// Union descriptor subtype.
#define CDCGenericDescriptor_UNION                              0x06

//------------------------------------------------------------------------------
/// \page "CDC Data Interface Values"
/// This page lists the values for CDC Data Interface Descriptor.
///
/// !Values
/// - CDCDataInterfaceDescriptor_CLASS
/// - CDCDataInterfaceDescriptor_SUBCLASS
/// - CDCDataInterfaceDescriptor_NOPROTOCOL

/// Interface class code for a data class interface.
#define CDCDataInterfaceDescriptor_CLASS        0x0A
/// Interface subclass code for a data class interface.
#define CDCDataInterfaceDescriptor_SUBCLASS     0x00
/// Protocol code for a data class interface which does not implement any
/// particular protocol.
#define CDCDataInterfaceDescriptor_NOPROTOCOL   0x00

//------------------------------------------------------------------------------
/// \page "CDC Request Codes"
/// This page lists USB CDC Request Codes.
///
/// !Codes
/// - CDCGenericRequest_SETLINECODING
/// - CDCGenericRequest_GETLINECODING
/// - CDCGenericRequest_SETCONTROLLINESTATE

/// SetLineCoding request code.
#define CDCGenericRequest_SETLINECODING             0x20
/// GetLineCoding request code.
#define CDCGenericRequest_GETLINECODING             0x21
/// SetControlLineState request code.
#define CDCGenericRequest_SETCONTROLLINESTATE       0x22
/// Break request code.
#define CDCGenericRequest_BREAK                     0x23






//! Device Status
#define BUS_POWERED                       0
#define SELF_POWERED                      1

//! Device State
#define ATTACHED                          0
#define POWERED                           1
#define DEFAULT                           2
#define ADDRESSED                         3
#define CONFIGURED                        4
#define SUSPENDED                         5

//! endpoint descriptors wrappers
#define USB_ENDPOINT_OUT(address) (address & 0xF)
#define USB_ENDPOINT_IN(address)  ((1 << 7) | (address & 0xF))

#define USB_ENDPOINT_SIZE(a)      (a)
#define USB_INTERVAL(a)           (a)

#define USB_LANGUAGE_ID           0x0409
#define USB_UNICODE(a)            (a), 0x00
#define USB_STRING_HEADER(size)   ((size * 2) + 2), USBGenericDescriptor_STRING

#endif /* _USB_H */


