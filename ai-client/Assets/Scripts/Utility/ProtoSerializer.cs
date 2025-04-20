using System;
using NetworkData;
using Google.Protobuf;
using Google.Protobuf.Reflection;

public class ProtoSerializer
{
    public static byte[] SerializeNetworkData(RpcPacket data)
    {
        return data.ToByteArray();
    }
}