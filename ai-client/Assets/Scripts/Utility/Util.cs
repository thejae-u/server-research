using System;
using System.Diagnostics;
using System.Net;
using Google.Protobuf;

namespace Utility
{
    public class Util
    {
        private static Stopwatch _stopwatch;

        public static void StartStopwatch()
        {
            _stopwatch = new Stopwatch();
            _stopwatch.Start();
        }
        
        public static string EndStopwatch()
        {
            if (_stopwatch == null)
            {
                return "Stopwatch not started.";
            }
            
            _stopwatch.Stop();
            TimeSpan ts = _stopwatch.Elapsed;
            return _stopwatch.ElapsedMilliseconds.ToString();
        }

        public static ushort ConvertByteStringToUShort(ByteString byteString)
        {
            byte[] bytesArray = byteString.ToByteArray();
            
            if(!BitConverter.IsLittleEndian)
                Array.Reverse(bytesArray);

            string portString = System.Text.Encoding.ASCII.GetString(bytesArray);
            
            if (!ushort.TryParse(portString, out ushort port))
            {
                throw new InvalidOperationException("Invalid port format");
            }

            return port;
        }
        
        public static ByteString ConvertUShortToByteString(ushort port)
        {
            byte[] bytesArray = BitConverter.GetBytes(port);
            return ByteString.CopyFrom(bytesArray);
        }
    }
}