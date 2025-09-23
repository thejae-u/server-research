using System;
using UnityEngine;

namespace Network
{
    public class LobbyNetworkManager : Singleton<LobbyNetworkManager>
    {
        [SerializeField] private string WEB_SERVER_IP = "127.0.0.1";
        [SerializeField] private ushort PORT = 8080;
        
        private void Awake()
        {
        }
    }   
}