//
// Copyright 2023, Colias Group, LLC
//
// SPDX-License-Identifier: BSD-2-Clause
//

#![no_std]
#![no_main]

use core::ptr::NonNull;

use virtio_drivers::{
    device::net::*,
    transport::{
        mmio::{MmioTransport, VirtIOHeader},
        DeviceType, Transport,
    },
};

use sel4_externally_shared::{ExternallySharedRef, ExternallySharedRefExt};
use sel4_microkit::{
    memory_region_symbol, protection_domain, var, Channel, Handler, Infallible,
};

use sddf_net_queue::{QueueHandle};

mod hal;

use hal::HalImpl;

const DEVICE: Channel = Channel::new(0);
const VIRT_RX: Channel = Channel::new(1);
const VIRT_TX: Channel = Channel::new(2);

pub const VIRTIO_NET_MMIO_OFFSET: usize = 0xe00;
pub const VIRTIO_NET_DRIVER_DMA_SIZE: usize = 0x200_000;
pub const VIRTIO_NET_CLIENT_DMA_SIZE: usize = 0x200_000;

const NET_QUEUE_SIZE: usize = 16;
const NET_BUFFER_LEN: usize = 2048;

#[protection_domain(
    heap_size = 512 * 1024,
)]
fn init() -> HandlerImpl {
    HalImpl::init(
        VIRTIO_NET_DRIVER_DMA_SIZE,
        *var!(virtio_net_driver_dma_vaddr: usize = 0),
        *var!(virtio_net_driver_dma_paddr: usize = 0),
    );

    let mut dev = {
        let header = NonNull::new(
            (*var!(virtio_net_mmio_vaddr: usize = 0) + VIRTIO_NET_MMIO_OFFSET)
                as *mut VirtIOHeader,
        )
        .unwrap();
        let transport = unsafe { MmioTransport::new(header) }.unwrap();
        assert_eq!(transport.device_type(), DeviceType::Network);
        VirtIONet::<HalImpl, MmioTransport, NET_QUEUE_SIZE>::new(transport, NET_BUFFER_LEN).unwrap()
    };

    let client_region = unsafe {
        ExternallySharedRef::<'static, _>::new(
            memory_region_symbol!(virtio_net_client_dma_vaddr: *mut [u8], n = VIRTIO_NET_CLIENT_DMA_SIZE),
        )
    };

    let rx_queue = QueueHandle::<'_>::new(
        unsafe { ExternallySharedRef::new(memory_region_symbol!(virtio_net_rx_free: *mut _)) },
        unsafe { ExternallySharedRef::new(memory_region_symbol!(virtio_net_rx_active: *mut _)) },
    );

    let tx_queue = QueueHandle::<'_>::new(
        unsafe { ExternallySharedRef::new(memory_region_symbol!(virtio_net_tx_free: *mut _)) },
        unsafe { ExternallySharedRef::new(memory_region_symbol!(virtio_net_tx_active: *mut _)) },
    );

    dev.ack_interrupt();
    DEVICE.irq_ack().unwrap();

    HandlerImpl {
        dev,
        client_region,
        rx_queue,
        tx_queue,
    }
}

struct HandlerImpl {
    dev: VirtIONet<HalImpl, MmioTransport, NET_QUEUE_SIZE>,
    client_region: ExternallySharedRef<'static, [u8]>,
    rx_queue: QueueHandle<'static>,
    tx_queue: QueueHandle<'static>,
}

impl Handler for HandlerImpl {
    type Error = Infallible;

    fn notified(&mut self, channel: Channel) -> Result<(), Self::Error> {
        match channel {
            DEVICE | VIRT_RX | VIRT_TX => {
                let mut notify_rx = false;

                while self.dev.can_recv() && !self.rx_queue.free_mut().is_empty() {
                    let rx_buf = self.dev.receive().unwrap();
                    let desc = self.rx_queue.free_mut().dequeue().unwrap();
                    let desc_len = usize::try_from(desc.len()).unwrap();
                    assert!(desc_len >= rx_buf.packet_len());
                    let buf_range = {
                        let start = desc.io_or_offset();
                        start..start + rx_buf.packet_len()
                    };
                    self.client_region
                        .as_mut_ptr()
                        .index(buf_range)
                        .copy_from_slice(rx_buf.packet());
                    self.dev.recycle_rx_buffer(rx_buf).unwrap();
                    self.rx_queue
                        .active_mut()
                        .enqueue(desc)
                        .unwrap();
                    notify_rx = true;
                }

                if notify_rx {
                    VIRT_RX.notify();
                }

                let mut notify_tx = false;

                while !self.tx_queue.free_mut().is_empty() && self.dev.can_send() {
                    let desc = self.tx_queue.free_mut().dequeue().unwrap();
                    let buf_range = {
                        let start = desc.io_or_offset();
                        start..start + usize::try_from(desc.len()).unwrap()
                    };
                    let mut tx_buf = self.dev.new_tx_buffer(buf_range.len());
                    self.client_region
                        .as_ptr()
                        .index(buf_range)
                        .copy_into_slice(tx_buf.packet_mut());
                    self.dev.send(tx_buf).unwrap();
                    self.tx_queue
                        .active_mut()
                        .enqueue(desc)
                        .unwrap();
                    notify_tx = true;
                }

                if notify_tx {
                    VIRT_TX.notify();
                }

                self.dev.ack_interrupt();
                DEVICE.irq_ack().unwrap();
            }
            _ => {
                unreachable!()
            }
        }
        Ok(())
    }
}

