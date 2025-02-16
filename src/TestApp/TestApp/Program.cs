using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using static TestApp.MainLogger;
using static TestApp.LogLevel;
using System.Net;
using System.Threading;

namespace TestApp
{
	internal class Program
	{
		static async Task Main(string[] args)
		{
			using (UdpClient udp = new UdpClient(new IPEndPoint(IPAddress.Any, 0)))
			{
				Log("Searching for devices...");
				RGB2Client rgb = new RGB2Client(udp);
				//int timeout = 4000;
				RGB2Device[] devs;
				do
				{
					Log("Pinging...", Debug2);
					devs = await RGB2Device.Ping(rgb, timeout: 4000, listenInterval: 1000);
				} while (devs.Length == 0);
				string print = $"Devices ({devs.Length}):\n";
				foreach (var d in devs)
					print += $" {d.EP.Address}\n";

				Log(print);

			}
		}
	}
}
