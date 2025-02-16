using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Text;
using System.Threading.Tasks;

namespace TestApp
{
	public static class Utils
	{
		public static Random rnd = new Random();
		public const uint ping_const = 0x413A0D53;
		public static readonly byte[] ping_const_b = { 0x41, 0x3A, 0x0D, 0x53 };
		public static IPAddress[] GetInterfaceAddresses()
		{
			var ips = from x in NetworkInterface.GetAllNetworkInterfaces()
					  where x.OperationalStatus == OperationalStatus.Up
					  select (from a in x.GetIPProperties().UnicastAddresses
							  where a.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork
							  select a.Address);
			return ips.SelectMany(x => x).ToArray();
		}

		public static IPAddress AsBroadcast(this IPAddress address)
		{
			if (address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
			{
				byte[] addr = address.GetAddressBytes();
				addr[3] = 255;
				return new IPAddress(addr);
			}
			throw new NotSupportedException("only ipv4 is supported");
		}
	}
}
