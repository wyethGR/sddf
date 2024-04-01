//
// Copyright 2023, Colias Group, LLC
//
// SPDX-License-Identifier: BSD-2-Clause
//

#![no_std]

use zerocopy::{AsBytes, FromBytes, FromZeroes};
use core::sync::atomic::Ordering;

use sel4_externally_shared::{
    map_field, ExternallySharedPtr, ExternallySharedPtrExt, ExternallySharedRef,
};

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq, AsBytes, FromBytes, FromZeroes)]
pub struct Descriptor {
    io_or_offset: usize,
    len: u16,
    _padding: [u8; 6],
}

impl Descriptor {
    pub fn new(io_or_offset: usize, len: u16) -> Self {
        Self {
            io_or_offset,
            len,
            _padding: [0; 6],
        }
    }

    pub fn io_or_offset(&self) -> usize {
        self.io_or_offset
    }

    pub fn set_io_or_offset(&mut self, io_or_offset: usize) {
        self.io_or_offset = io_or_offset;
    }

    #[allow(clippy::len_without_is_empty)]
    pub fn len(&self) -> u16 {
        self.len
    }

    pub fn set_len(&mut self, len: u16) {
        self.len = len;
    }
}

pub const QUEUE_SIZE: usize = 512 * 3;

pub struct QueueHandle<'a, T = Descriptor> {
    free: Queue<'a, T>,
    active: Queue<'a, T>,
}

impl<'a, T: Copy> QueueHandle<'a, T> {
    pub fn new(
        free: ExternallySharedRef<'a, RawQueue<T>>,
        active: ExternallySharedRef<'a, RawQueue<T>>,
    ) -> Self {
        let free = Queue::new(free);
        let active = Queue::new(active);
        Self { free, active }
    }

    pub fn free(&self) -> &Queue<'a, T> {
        &self.free
    }

    pub fn active(&self) -> &Queue<'a, T> {
        &self.active
    }

    pub fn free_mut(&mut self) -> &mut Queue<'a, T> {
        &mut self.free
    }

    pub fn active_mut(&mut self) -> &mut Queue<'a, T> {
        &mut self.active
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct RawQueue<T = Descriptor> {
    pub tail: u16,
    pub head: u16,
    pub consumer_signalled: u32,
    pub buffers: [T; QUEUE_SIZE],
}

pub struct Queue<'a, T = Descriptor> {
    queue: ExternallySharedRef<'a, RawQueue<T>>,
}

impl<'a, T: Copy> Queue<'a, T> {
    const SIZE: usize = QUEUE_SIZE;

    pub fn new(queue: ExternallySharedRef<'a, RawQueue<T>>) -> Self {
        Self {
            queue
        }
    }

    pub const fn capacity(&self) -> usize {
        Self::SIZE - 1
    }

    pub fn tail(&mut self) -> ExternallySharedPtr<'_, u16> {
        let ptr = self.queue.as_mut_ptr();
        map_field!(ptr.tail)
    }

    pub fn head(&mut self) -> ExternallySharedPtr<'_, u16> {
        let ptr = self.queue.as_mut_ptr();
        map_field!(ptr.head)
    }

    fn buffer(&mut self, index: u16) -> ExternallySharedPtr<'_, T> {
        let residue = self.residue(index);
        let ptr = self.queue.as_mut_ptr();
        map_field!(ptr.buffers).as_slice().index(residue)
    }

    pub fn is_empty(&mut self) -> bool {
        (self.tail().read() - self.head().read()) == 0
    }

    pub fn is_full(&mut self) -> bool {
        (self.tail().read() + 1 - self.head().read()) % Self::SIZE as u16 == 0
    }

    pub fn size(&mut self) -> usize {
        (self.tail().read() - self.head().read()) as usize
    }

    fn residue(&self, index: u16) -> usize {
        usize::try_from(index).unwrap() % Self::SIZE
    }
}

impl<'a, T: Copy + FromBytes + AsBytes> Queue<'a, T> {
    pub fn enqueue(
        &mut self,
        desc: T,
    ) -> Result<(), T> {
        if self.is_full() {
            return Err(desc);
        }
        let tail = self.tail().read();
        self.buffer(tail).write(desc);
        self.tail()
            .atomic()
            .store(tail + 1, Ordering::Release);
        Ok(())
    }

    pub fn dequeue(&mut self) -> Option<T> {
        if self.is_empty() {
            return None;
        }
        let head = self.head().read();
        let desc = self.buffer(head).read();
        self.head()
            .atomic()
            .store(head + 1, Ordering::Release);
        Some(desc)
    }
}
