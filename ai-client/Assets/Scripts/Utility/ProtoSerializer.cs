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

    public static Guid ConvertUuidToGuid(ByteString uuid)
    {
        return uuid.Length != 16 ? Guid.Empty : new Guid(uuid.ToByteArray());
    }
    
    public static ByteString SerializeUuid(Guid guid)
    {
        return ByteString.CopyFrom(guid.ToByteArray());
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