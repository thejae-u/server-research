using System;
using System.Diagnostics;
using Google.Protobuf.WellKnownTypes;

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

        public static string ConvertTimestampToString(Timestamp timestamp)
        {
            if (timestamp == null)
            {
                return string.Empty;
            }

            var dateTime = timestamp.ToDateTime();
            return dateTime.ToString("yyyy-MM-dd HH:mm:ss");
        } 
    }
}