using System;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Threading;


namespace example2
{
    public static class DDX
    {
        public const int DDX_CONTINUE_RECORDING = 0;

        [StructLayout(LayoutKind.Sequential)]
        public class FrameData // class instead of struct so it could be null
        {
            public IntPtr buffer;
            public int rowPitch;
            public int dxgiFormat; // most likely DXGI_FORMAT_B8G8R8A8_*
            public int width;
            public int height;
        }

        public delegate int OnFrameCallback(FrameData frame, object opq);

        [DllImport("ddx.dll")]
        public static extern int ddx_context_size();

        [DllImport("ddx.dll")]
        public static extern int ddx_init(IntPtr context);

        [DllImport("ddx.dll")]
        public static extern int ddx_record(IntPtr context, OnFrameCallback onFrame, object opq);

        [DllImport("ddx.dll")]
        public static extern int ddx_cleanup(IntPtr context);
    }

    class Program
    {
        static void Main(string[] args)
        {
            var bufsize = DDX.ddx_context_size();
            var record_context = Marshal.AllocHGlobal(bufsize);
            var err = 0;
            var out_dir = "C:\\windows\\temp\\ddx";
            var output_format = ImageFormat.Jpeg; // ImageFormat.Bmp should be less cpu intesive
            uint max_frames = 200;
            var ext = output_format.ToString().ToLower();

            err = DDX.ddx_init(record_context);
            if (0 == err)
            {
                uint frame_count = 0;

                err = DDX.ddx_record(record_context, delegate (DDX.FrameData frame, object opq)
                {
                    if (frame == null)
                    {
                        Thread.Sleep(200);
                        return DDX.DDX_CONTINUE_RECORDING; // not dirty                  
                    }

                    if (++frame_count > max_frames)
                        return 1; // stop

                    // assuming format is DXGI_FORMAT_B8G8R8A8_*
                    var output = new Bitmap(frame.width, frame.height, frame.rowPitch, PixelFormat.Format32bppArgb, frame.buffer);
                    output.Save(Path.Combine(out_dir, $"{frame_count}.{ext}"), output_format);

                    Thread.Sleep(100);
                    return DDX.DDX_CONTINUE_RECORDING;

                }, IntPtr.Zero);

                if (err != 0)
                {
                    Console.WriteLine($"failed {err}");
                }
            }

            DDX.ddx_cleanup(record_context);
            Marshal.FreeHGlobal(record_context);
        }
    }
}
