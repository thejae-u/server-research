using System;
using System.Linq;
using NetworkData;
using Google.Protobuf;
using Google.Protobuf.Reflection;
using Google.Protobuf.WellKnownTypes;

public class ProtoSerializer
{
    public static byte[] SerializeNetworkData(RpcPacket data)
    {
        return data.ToByteArray();
    }

    public static RpcPacket DeserializeNetworkData(byte[] data)
    {
        return data.TakeWhile(c => c == 0).Any() ? null : RpcPacket.Parser.ParseFrom(data);
    }

    public static Guid ConvertUuidToGuid(string uid)
    {
        return uid.Length != 16 ? Guid.Empty : new Guid(uid.ToString());
    }

    public static string SerializeUuid(Guid guid)
    {
        return guid.ToString();
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