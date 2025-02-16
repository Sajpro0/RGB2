using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace TestApp
{
	public static class IDs
	{
		public const byte
			PROTO_SIG = 159,
			COMMAND_RESV = 0,
			COMMAND_PING = 1,
			COMMAND_RGBV = 2,
			COMMAND_CONNRESET = 3,
			COMMAND_READREG = 8,
			COMMAND_WRITEREG = 9,
			REPLY_RESV = 0,
			REPLY_RESPONSE = 1,
			REPLY_ERROR = 15;
		public const ushort 
			UDP_LISTEN_PORT = 39368;
	}
}
