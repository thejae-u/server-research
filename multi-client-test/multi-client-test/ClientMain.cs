using System;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using multi_client_test;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

class ClientMain
{
    static async Task Main(string[] args)
    {
        string ip = "127.0.0.1";
        ushort port = 53200;
        
        HashSet<Client> clients = [];
        var clientTasks = new List<Task>();

        
        for (var i = 0; i < 50; ++i)
        {
            var uuid = Guid.NewGuid().ToString();
            var client = new Client(ip, port, uuid);

            if (!clients.Add(client))
            {
                Console.WriteLine($"Client {uuid} already exists.");
                continue;
            }
            
            Console.WriteLine($"Client {uuid} created.");
            clientTasks.Add(client.AsyncConnect());
        }

        Console.WriteLine($"Press any key to stop all clients...");
        Task.WaitAny(Task.Run(() => Console.ReadKey(true)));
        
        var disconnectTasks = new List<Task>();
        foreach (var client in clients)
        {
            if(!client.IsOnline)
                continue;
            
            disconnectTasks.Add(client.AsyncDisconnect());
            Console.WriteLine($"Client {client.Uuid} disconnecting...");
        }
        
        await Task.WhenAll(disconnectTasks);
        Console.WriteLine("All Done.");
    }
}