import java.io.*;
import java.nio.*;
import java.nio.channels.*;

MappedByteBuffer mapping;

void setup()
{
  size(640, 480);

  // The tracking buffer is a file on disk that, when we "map" it into memory,
  // allows us to access the same memory that the SpeedyEye app is writing to.

  try {
    FileChannel fc = new RandomAccessFile(dataFile("tracking-buffer.bin"), "rw").getChannel();
    mapping = fc.map(FileChannel.MapMode.READ_WRITE, 0, fc.size());
    mapping.order(ByteOrder.LITTLE_ENDIAN);
  } catch (IOException e) {
    println(e);
    assert(false);
  }
}

void draw()
{
  // Read the total motion from the buffer's header,
  // and wrap around at the edges of the screen.

  float totalX = mapping.getFloat(0) % width;
  if (totalX < 0) totalX += width;

  float totalY = mapping.getFloat(4) % height;
  if (totalY < 0) totalY += height;

  clear();
  rect(totalX, totalY, 20, 20);
}

