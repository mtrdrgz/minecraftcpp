package com.mojang.blaze3d.vertex;

import com.mojang.blaze3d.GraphicsWorkarounds;
import com.mojang.blaze3d.buffers.GpuBuffer;
import com.mojang.blaze3d.systems.CommandEncoder;
import com.mojang.blaze3d.systems.GpuDevice;
import com.mojang.datafixers.util.Pair;
import it.unimi.dsi.fastutil.objects.Object2ObjectOpenHashMap;
import it.unimi.dsi.fastutil.objects.ObjectIterator;
import it.unimi.dsi.fastutil.objects.ObjectOpenHashSet;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Map.Entry;
import net.minecraft.client.renderer.MappableRingBuffer;
import net.minecraft.util.VisibleForDebug;
import net.minecraft.util.profiling.Profiler;
import net.minecraft.util.profiling.Zone;
import org.jspecify.annotations.Nullable;
import org.lwjgl.system.MemoryUtil;

public class UberGpuBuffer<T> implements AutoCloseable {
   private final int alignSize;
   private final UberGpuBuffer.UberGpuBufferStagingBuffer stagingBuffer;
   private int stagingBufferUsedSize;
   private final String name;
   private final List<Pair<TlsfAllocator, UberGpuBuffer.UberGpuBufferHeap>> nodes;
   private final Object2ObjectOpenHashMap<T, UberGpuBuffer.StagedAllocationEntry<T>> stagedAllocations = new Object2ObjectOpenHashMap(32);
   private final ObjectOpenHashSet<T> skippedStagedAllocations = new ObjectOpenHashSet(32);
   private final Map<T, TlsfAllocator.Allocation> allocationMap = new HashMap<>(256);

   public UberGpuBuffer(
      final String name,
      final int usage,
      final int heapSize,
      final int alignSize,
      final GpuDevice gpuDevice,
      final int stagingBufferSize,
      final GraphicsWorkarounds workarounds
   ) {
      if (stagingBufferSize > heapSize) {
         throw new IllegalArgumentException("Staging buffer size cannot be bigger than heap size");
      }

      this.name = "UberBuffer " + name;
      this.stagingBuffer = UberGpuBuffer.UberGpuBufferStagingBuffer.create(this.name, gpuDevice, stagingBufferSize, workarounds);
      this.stagingBufferUsedSize = 0;
      this.nodes = new ArrayList<>();
      this.alignSize = alignSize;
      String initialHeapName = this.name + " 0";
      UberGpuBuffer.UberGpuBufferHeap initialHeap = new UberGpuBuffer.UberGpuBufferHeap(heapSize, gpuDevice, usage, initialHeapName);
      TlsfAllocator initialTlsfAllocator = new TlsfAllocator(initialHeap);
      this.nodes.add(new Pair(initialTlsfAllocator, initialHeap));
   }

   public boolean addAllocation(final T allocationKey, final UberGpuBuffer.@Nullable UploadCallback<T> callback, final ByteBuffer buffer) {
      int startOffset = this.stagingBufferUsedSize;
      ByteBuffer stagingBuffer = this.stagingBuffer.getStagingBuffer();
      if (buffer.remaining() > stagingBuffer.capacity()) {
         throw new IllegalArgumentException("UberGpuBuffer cannot have any allocations bigger than its staging buffer, increase the staging buffer size!");
      }

      if (buffer.remaining() > stagingBuffer.capacity() - startOffset) {
         return false;
      }

      MemoryUtil.memCopy(buffer, stagingBuffer.position(startOffset));
      this.stagingBufferUsedSize = this.stagingBufferUsedSize + buffer.remaining();
      UberGpuBuffer.StagedAllocationEntry<T> entry = new UberGpuBuffer.StagedAllocationEntry<>(callback, startOffset, buffer.remaining());
      this.stagedAllocations.put(allocationKey, entry);
      return true;
   }

   public boolean uploadStagedAllocations(final GpuDevice gpuDevice, final CommandEncoder encoder) {
      ObjectIterator newHeapCreatedOrDestroyed = this.stagedAllocations.keySet().iterator();

      while (newHeapCreatedOrDestroyed.hasNext()) {
         T key = (T)newHeapCreatedOrDestroyed.next();
         this.freeAllocation(key);
      }

      boolean newHeapCreatedOrDestroyedx = false;
      Zone ignored = Profiler.get().zone("Upload staged allocations");

      try {
         ObjectIterator node = this.stagedAllocations.entrySet().iterator();

         while (node.hasNext()) {
            Entry<T, UberGpuBuffer.StagedAllocationEntry<T>> entry = (Entry<T, UberGpuBuffer.StagedAllocationEntry<T>>)node.next();
            long allocationSize = entry.getValue().size;
            if (!this.skippedStagedAllocations.contains(entry.getKey())) {
               TlsfAllocator.Allocation allocation = null;

               for (Pair<TlsfAllocator, UberGpuBuffer.UberGpuBufferHeap> nodex : this.nodes) {
                  allocation = ((TlsfAllocator)nodex.getFirst()).allocate(allocationSize, this.alignSize);
                  if (allocation != null) {
                     break;
                  }
               }

               if (allocation == null) {
                  Zone ignored2 = Profiler.get().zone("Create new heap");

                  try {
                     UberGpuBuffer.UberGpuBufferHeap firstHeap = (UberGpuBuffer.UberGpuBufferHeap)this.nodes.getFirst().getSecond();
                     long heapSize = firstHeap.gpuBuffer.size();
                     assert allocationSize <= heapSize;
                     String heapName = String.format(Locale.ROOT, "%s %d", this.name, this.nodes.size());
                     UberGpuBuffer.UberGpuBufferHeap newHeap = new UberGpuBuffer.UberGpuBufferHeap(heapSize, gpuDevice, firstHeap.gpuBuffer.usage(), heapName);
                     TlsfAllocator newTlsfAllocator = new TlsfAllocator(newHeap);
                     this.nodes.add(new Pair(newTlsfAllocator, newHeap));
                     allocation = newTlsfAllocator.allocate(allocationSize, this.alignSize);
                     newHeapCreatedOrDestroyedx = true;
                  } catch (Throwable var19) {
                     if (ignored2 != null) {
                        try {
                           ignored2.close();
                        } catch (Throwable var18) {
                           var19.addSuppressed(var18);
                        }
                     }

                     throw var19;
                  }

                  if (ignored2 != null) {
                     ignored2.close();
                  }
               }

               if (allocation != null) {
                  TlsfAllocator.Heap allocationHeap = allocation.getHeap();
                  GpuBuffer allocationDestBuffer = ((UberGpuBuffer.UberGpuBufferHeap)allocationHeap).gpuBuffer;
                  this.stagingBuffer.copyToHeap(encoder, allocationDestBuffer, allocation.getOffsetFromHeap(), entry.getValue().offset, allocationSize);
                  this.allocationMap.put(entry.getKey(), allocation);
                  if (entry.getValue().callback != null) {
                     entry.getValue().callback.bufferHasBeenUploaded(entry.getKey());
                  }
               }
            }
         }

         this.stagingBuffer.clearFrame(encoder);
         this.stagingBufferUsedSize = 0;
         this.stagedAllocations.clear();
         this.skippedStagedAllocations.clear();
      } catch (Throwable var20) {
         if (ignored != null) {
            try {
               ignored.close();
            } catch (Throwable var17) {
               var20.addSuppressed(var17);
            }
         }

         throw var20;
      }

      if (ignored != null) {
         ignored.close();
      }

      Iterator<Pair<TlsfAllocator, UberGpuBuffer.UberGpuBufferHeap>> iterator = this.nodes.iterator();

      while (iterator.hasNext() && this.nodes.size() > 1) {
         Pair<TlsfAllocator, UberGpuBuffer.UberGpuBufferHeap> node = iterator.next();
         if (((TlsfAllocator)node.getFirst()).isCompletelyFree()) {
            ((UberGpuBuffer.UberGpuBufferHeap)node.getSecond()).gpuBuffer.close();
            iterator.remove();
            newHeapCreatedOrDestroyedx = true;
            break;
         }
      }

      return newHeapCreatedOrDestroyedx;
   }

   public TlsfAllocator.@Nullable Allocation getAllocation(final T allocationKey) {
      return this.allocationMap.get(allocationKey);
   }

   public void removeAllocation(final T allocationKey) {
      this.skippedStagedAllocations.add(allocationKey);
      this.freeAllocation(allocationKey);
   }

   private void freeAllocation(final T allocationKey) {
      TlsfAllocator.Allocation allocation = this.allocationMap.remove(allocationKey);
      if (allocation != null) {
         for (Pair<TlsfAllocator, UberGpuBuffer.UberGpuBufferHeap> node : this.nodes) {
            if (node.getSecond() == allocation.getHeap()) {
               ((TlsfAllocator)node.getFirst()).free(allocation);
               break;
            }
         }
      }
   }

   public GpuBuffer getGpuBuffer(final TlsfAllocator.Allocation allocation) {
      return ((UberGpuBuffer.UberGpuBufferHeap)allocation.getHeap()).gpuBuffer;
   }

   @VisibleForDebug
   public void printStatistics() {
      for (int i = 0; i < this.nodes.size(); i++) {
         Pair<TlsfAllocator, UberGpuBuffer.UberGpuBufferHeap> node = this.nodes.get(i);
         String heapName = String.format(Locale.ROOT, "%s %d", this.name, i);
         ((TlsfAllocator)node.getFirst()).printAllocatorStatistics(heapName);
      }
   }

   @Override
   public void close() {
      this.stagingBuffer.destroyBuffer();
      this.stagingBufferUsedSize = 0;
      this.stagedAllocations.clear();
      this.allocationMap.clear();

      for (Pair<TlsfAllocator, UberGpuBuffer.UberGpuBufferHeap> node : this.nodes) {
         ((UberGpuBuffer.UberGpuBufferHeap)node.getSecond()).gpuBuffer.close();
      }

      this.nodes.clear();
   }

   private static class StagedAllocationEntry<T> {
      UberGpuBuffer.@Nullable UploadCallback<T> callback;
      long offset;
      long size;

      private StagedAllocationEntry(final UberGpuBuffer.@Nullable UploadCallback<T> callback, final long offset, final long size) {
         this.offset = offset;
         this.size = size;
         this.callback = callback;
      }
   }

   public static class UberGpuBufferHeap extends TlsfAllocator.Heap {
      GpuBuffer gpuBuffer;

      UberGpuBufferHeap(final long size, final GpuDevice gpuDevice, final int usage, final String name) {
         super(size);
         this.gpuBuffer = gpuDevice.createBuffer(() -> name, usage | 8 | 16, size);
      }
   }

   private abstract static class UberGpuBufferStagingBuffer {
      public static UberGpuBuffer.UberGpuBufferStagingBuffer create(
         final String name, final GpuDevice gpuDevice, final int stagingBufferSize, final GraphicsWorkarounds workarounds
      ) {
         return !workarounds.isGlOnDx12()
            ? new UberGpuBuffer.UberGpuBufferStagingBuffer.CPUStagingBuffer(name, gpuDevice, stagingBufferSize)
            : new UberGpuBuffer.UberGpuBufferStagingBuffer.MappedStagingBuffer(name, gpuDevice, stagingBufferSize);
      }

      abstract ByteBuffer getStagingBuffer();

      abstract void copyToHeap(final CommandEncoder encoder, final GpuBuffer heapBuffer, long heapOffset, long stagingBufferOffset, long copySize);

      abstract void clearFrame(final CommandEncoder encoder);

      abstract void destroyBuffer();

      private static class CPUStagingBuffer extends UberGpuBuffer.UberGpuBufferStagingBuffer {
         private final ByteBuffer stagingBuffer;

         private CPUStagingBuffer(final String name, final GpuDevice gpuDevice, final int stagingBufferSize) {
            this.stagingBuffer = MemoryUtil.memAlloc(stagingBufferSize);
         }

         @Override
         ByteBuffer getStagingBuffer() {
            return this.stagingBuffer;
         }

         @Override
         void copyToHeap(final CommandEncoder encoder, final GpuBuffer heapBuffer, final long heapOffset, final long stagingBufferOffset, final long copySize) {
            encoder.writeToBuffer(heapBuffer.slice(heapOffset, copySize), this.stagingBuffer.slice((int)stagingBufferOffset, (int)copySize));
         }

         @Override
         void clearFrame(final CommandEncoder encoder) {
            this.stagingBuffer.clear();
         }

         @Override
         void destroyBuffer() {
            this.stagingBuffer.clear();
            MemoryUtil.memFree(this.stagingBuffer);
         }
      }

      private static class MappedStagingBuffer extends UberGpuBuffer.UberGpuBufferStagingBuffer {
         private final MappableRingBuffer mappableRingBuffer;
         private GpuBuffer.MappedView currentMappedView;
         private GpuBuffer currentGPUBuffer;
         private ByteBuffer currentBuffer;

         private MappedStagingBuffer(final String name, final GpuDevice gpuDevice, final int stagingBufferSize) {
            String stagingBufferName = name + " staging buffer";
            this.mappableRingBuffer = new MappableRingBuffer(() -> stagingBufferName, 18, stagingBufferSize / 2);
            CommandEncoder encoder = gpuDevice.createCommandEncoder();
            this.currentGPUBuffer = this.mappableRingBuffer.currentBuffer();
            this.currentMappedView = encoder.mapBuffer(this.currentGPUBuffer, false, true);
            this.currentBuffer = this.currentMappedView.data();
         }

         @Override
         ByteBuffer getStagingBuffer() {
            return this.currentBuffer;
         }

         @Override
         void copyToHeap(final CommandEncoder encoder, final GpuBuffer heapBuffer, final long heapOffset, final long stagingBufferOffset, final long copySize) {
            encoder.copyToBuffer(this.currentGPUBuffer.slice(stagingBufferOffset, copySize), heapBuffer.slice(heapOffset, copySize));
         }

         @Override
         void clearFrame(final CommandEncoder encoder) {
            this.currentMappedView.close();
            this.mappableRingBuffer.rotate();
            this.currentGPUBuffer = this.mappableRingBuffer.currentBuffer();
            this.currentMappedView = encoder.mapBuffer(this.currentGPUBuffer, false, true);
            this.currentBuffer = this.currentMappedView.data();
         }

         @Override
         void destroyBuffer() {
            this.currentMappedView.close();
            this.mappableRingBuffer.close();
         }
      }
   }

   public interface UploadCallback<T> {
      void bufferHasBeenUploaded(T key);
   }
}
