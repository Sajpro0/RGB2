using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using static TestApp.MainLogger;
using static TestApp.LogLevel;

namespace TestApp
{
	public class RGB2Device
	{
		public RGB2Client cl; // the udp client used

		public IPEndPoint EP;

		internal RGB2Packet_DEV ReceivedPacket; 

		public RGB2Device(RGB2Client c, IPEndPoint ep)
		{
			this.cl = c;
			EP = ep;
			Reset();
		}

		



		public void SendPacket(byte[] data)
		{

		}
		public bool A; // if the "A" bit is set, default should be 0

		public void Reset()
		{
			A = false;
			ReceivedPacket = null;
		}

		public async static Task<RGB2Device[]> Ping(RGB2Client cl, IPEndPoint address = null, int timeout = 4000, int listenInterval = 4000)
		{
			using (CancellationTokenSource cts = new CancellationTokenSource())
			{
				cts.CancelAfter(timeout);
				return await Ping(cl, address, cts.Token, listenInterval);
			}
		}

		static Logger Ping_log = Logger.Main.Mine<RGB2Device>("ping", Debug3);

		public static async Task<RGB2Device[]> Ping(RGB2Client cl, IPEndPoint address = null, CancellationToken ct = default, int listenInterval = 4000)
		{
			Ping_log.Log($"Ping called with ep = {address?.ToString() ?? "NULL"}");
			byte[] random = new byte[4];
			Utils.rnd.NextBytes(random);
			byte[] result = new byte[random.Length];
			for (int i = 0; i < random.Length; i++)
			{
				result[i] = (byte)(random[i] ^ Utils.ping_const_b[i]);
			}

			Ping_log.Log(
				$"gonna ping with 0x{random[0]:X2}{random[1]:X2}{random[2]:X2}{random[3]:X2}\n" +
				$"      expecting 0x{result[0]:X2}{result[1]:X2}{result[2]:X2}{result[3]:X2}", Debug2);

			void ping_internal(IPEndPoint ep, bool A = false)
			{
				byte[] buf = new byte[1 + 1 + 4];
				buf[0] = IDs.PROTO_SIG;
				buf[1] = IDs.COMMAND_PING;
				if (A)
					buf[1] |= 0b10000000;
				for (int i = 0; i < 4; i++)
				{
					buf[i + 2] = random[i];
				}
				cl.udp.Send(buf, buf.Length, ep);
				Ping_log.Log($"sent ping packet to {ep}, A={(A ? "1" : "0")}");
			}
			Stopwatch lsw = null;
			int timeLeft = listenInterval;
			bool to(bool full = false)
			{
				var t = full ? listenInterval : (listenInterval / 2);
				int elaps = (int)lsw.ElapsedMilliseconds;
				timeLeft = Math.Max(0, (t - (int)lsw.ElapsedMilliseconds));
				if (timeLeft <= 0)
				{
					return false;
				}

				return true;
			}
			if (address == null)
			{
				// we want to scan and return a list of devices
				List<RGB2Device> devs = new List<RGB2Device>();


				try
				{
					var eps = (from x in Utils.GetInterfaceAddresses()
							   select new IPEndPoint(x.AsBroadcast(), IDs.UDP_LISTEN_PORT)).ToArray();
					Ping_log.Log($"scanning on {eps.Length} interfaces...", Debug2);
					do
					{
						lsw = Stopwatch.StartNew();

						ct.ThrowIfCancellationRequested();
						foreach (var ep in eps)
							ping_internal(ep);
						using (CancellationTokenSource cts = CancellationTokenSource.CreateLinkedTokenSource(ct))
						{
							try
							{
								if (to())
								{
									cts.CancelAfter(timeLeft);
									Ping_log.Log("cancel after1 " + timeLeft);
									do
									{
										cts.Token.ThrowIfCancellationRequested();
										var pkt = await cl.ReceivePacket(cts.Token);
										if (pkt == null || pkt.cmd != IDs.REPLY_RESPONSE || pkt.A)
										{
											Ping_log.Log("unusable packet discarded", Debug3);
											continue;
										}
										if (devs.Any(x => x.EP.Equals(cl.ReceivedEP))) // already have this device
											continue;
										var dev = new RGB2Device(cl, cl.ReceivedEP);
										dev.A = true; // flip the A bit
										devs.Add(dev);
										Ping_log.Log($"found device on {dev.EP}!", LogLevel.Debug);
									} while (to());
								}
							}
							catch (OperationCanceledException)
							{

							}
							catch
							{
								throw;
							}
						}
						
						ct.ThrowIfCancellationRequested();
						foreach (var ep in eps)
							ping_internal(ep, true);
						using (CancellationTokenSource cts = CancellationTokenSource.CreateLinkedTokenSource(ct))
						{
							try
							{
								if (to(true))
								{
									cts.CancelAfter(timeLeft);
									Ping_log.Log("cancel after2 " + timeLeft);
									do
									{
										cts.Token.ThrowIfCancellationRequested();
										var pkt = await cl.ReceivePacket(cts.Token);
										if (pkt == null || pkt.cmd != IDs.REPLY_RESPONSE || !pkt.A)
										{
											Ping_log.Log("unusable packet discarded");
											continue;
										}
										if (devs.Any(x => x.EP.Equals(cl.ReceivedEP))) // already have this device
											continue;
										var dev = new RGB2Device(cl, cl.ReceivedEP);
										devs.Add(dev);
										Ping_log.Log($"found device on {dev.EP}!", LogLevel.Debug);
									} while (to(true));
								}
							}
							catch (OperationCanceledException)
							{

							}
							catch
							{
								throw;
							}
						}
							
					} while (true);
					
				}
				catch (OperationCanceledException)
				{
					Ping_log.Log($"timed out", Debug2);
					return devs.ToArray();
				}
				catch (Exception e)
				{
					throw;
				}
			}
			else
			{
				try
				{
					do
					{
						lsw = Stopwatch.StartNew();
						// regular pinging of a specific device
						Ping_log.Log($"searching for device on {address}", LogLevel.Debug);
						ping_internal(address);

						while (to())
						{
							var pkt = await cl.ReceivePacket(timeLeft);
							if (pkt == null || pkt.cmd != IDs.REPLY_RESPONSE || !cl.ReceivedEP.Equals(address) || pkt.A)
							{
								Ping_log.Log("unusable packet discarded");
								continue;
							}
							var dev = new RGB2Device(cl, cl.ReceivedEP);
							dev.A = true; // flip the A bit
							Ping_log.Log($"found searched-for device on {dev.EP}", LogLevel.Debug);
							return new RGB2Device[] { dev };
						}
						ping_internal(address, true);
						while (to(true))
						{
							var pkt = await cl.ReceivePacket(timeLeft);
							if (pkt == null || pkt.cmd != IDs.REPLY_RESPONSE || !cl.ReceivedEP.Equals(address) || !pkt.A)
							{
								Ping_log.Log("unusable packet discarded");
								continue;
							}
							var dev = new RGB2Device(cl, cl.ReceivedEP);
							Ping_log.Log($"found searched-for device on {dev.EP}", LogLevel.Debug);
							return new RGB2Device[] { dev };
						}
					} while (true);
				}
				catch (OperationCanceledException)
				{
					Ping_log.Log($"timed out searching for device", Debug2);
					return null;
				}
				catch
				{
					throw;
				}
			}
		}
	}
	public class RGB2Client
	{
		public UdpClient udp;

		public RGB2Client(UdpClient udp)
		{
			this.udp = udp;
		}
		public async Task<RGB2Device> ReceivePacket(IEnumerable<RGB2Device> devices, CancellationToken ct = default)
		{
			while (true)
			{
				var r = await ReceivePacket(ct);
				if (r != null)
				{
					foreach (var d in devices)
					{
						if (d.EP.Equals(ReceivedEP))
						{
							d.ReceivedPacket = r;
							Log($"found matching device for ep = {ReceivedEP}", Debug3);
							return d;
						}
					}
				}
			}
		}
		Logger
			process_packet_log = Logger.Main.Mine<RGB2Client>(nameof(process_packet), Debug3),
			ReceivePacket_log = Logger.Main.Mine<RGB2Client>(nameof(ReceivePacket), Debug3),
			OnReceive_log = Logger.Main.Mine<RGB2Client>(nameof(OnReceive), Debug3);
		RGB2Packet_DEV process_packet()
		{
			process_packet_log.Log("processing received packet...");
			if (ReceivedBuf != null)
			{
				RGB2Packet_DEV pkt = RGB2Packet_DEV.Get(ReceivedBuf);
				ReceivedBuf = null;
				if (pkt != null)
				{
					process_packet_log.Log("packet ok");
					return pkt;
				}
			}
			process_packet_log.Log("bad packet");
			return null;
		}

		bool receiving = false;
		object receiveLock = new object();
		SemaphoreSlim 
			recSema = new SemaphoreSlim(1),
			recResultSema = new SemaphoreSlim(1), // wait handle for result, await it when waiting for result
			recReadSema = new SemaphoreSlim(1); // lock on receiving status to prevent it from changing
		public async Task<RGB2Packet_DEV> ReceivePacket(CancellationToken ct)
		{
			await recSema.WaitAsync(ct);
			try
			{
				await recReadSema.WaitAsync(ct); // keep outside of try to prevent release from being called in case this throws
				try
				{
					receiving = true;
				}
				finally
				{
					recReadSema.Release(); // release so OnReceive can check
				}

				ReceivePacket_log.Log($"attempting to receive packet");
				byte[] recBuf = new byte[0xFFFF];

				Exception callbackEx = null;

				while (true)
				{
					AsyncCallback cb = x =>
					{
						try
						{
							ReceivePacket_log.Log("receive callback triggerred");
							try
							{
								OnReceive(udp, x);
							}
							catch (Exception ex)
							{
								// something went wrong
								callbackEx = ex;
								return;
							}
							
						}
						finally
						{
							recResultSema.Release();
							ReceivePacket_log.Log($"end receive");
						}
					};
					ct.ThrowIfCancellationRequested(); // throw in case timeout was reached

					await recResultSema.WaitAsync(ct); // await for previous result, synchronous if there is none pending, packet might be invalid and we are requested to restart BeginReceive
					try
					{
						var d = process_packet(); // attempt to process a packet previously received
						if (d != null)
						{
							recResultSema.Release();
							return d; // valid packet, return immediately
						}
						ReceivePacket_log.Log($"begin receive");
						EndPoint ep = new IPEndPoint(IPAddress.Any, IDs.UDP_LISTEN_PORT);
						while (true) // ensure read is pending
						{
							try
							{
								lock (udp)
								{
									udp.BeginReceive(cb, null); // no exception => callback will run
								}
								// go out of loop
								ReceivePacket_log.Log("started BeginReceive");
							}
							catch (SocketException ex)
							{
								if (ex.ErrorCode == 10054)
								{
									ReceivePacket_log.Log($"suppressing Socket Error {ex.ErrorCode}:\n" + ex.Message);
									continue;
								}
								throw;
							}
							catch
							{
								throw;
							}
							break;
						}
					}
					catch
					{
						recResultSema.Release(); // release ourselves as there is no read pending
						throw;
					}
				}

			}
			finally
			{
				ReceivePacket_log.Log("releasing read lock");
				recSema.Release();
			}
		}

		public async Task<RGB2Packet_DEV> ReceivePacket(int timeout)
		{
			using (CancellationTokenSource cts = new CancellationTokenSource())
			{
				cts.CancelAfter(timeout);
				return await ReceivePacket(cts.Token);
			}
		}

		public async Task<RGB2Device> ReceivePacket(IEnumerable<RGB2Device> devices, int timeout)
		{
			using (CancellationTokenSource cts = new CancellationTokenSource())
			{
				cts.CancelAfter(timeout);
				return await ReceivePacket(devices, cts.Token);
			}
		}

		byte[] ReceivedBuf = null; // single packet buffer for incoming packets, null when no packet available
		public IPEndPoint ReceivedEP;
		public IPEndPoint ListenEP = new IPEndPoint(IPAddress.Any, IDs.UDP_LISTEN_PORT);
		void OnReceive(UdpClient c, IAsyncResult r)
		{
			try
			{
				ReceivedEP = new IPEndPoint(ListenEP.Address, ListenEP.Port);
				ReceivedBuf = c.EndReceive(r, ref ReceivedEP);
				OnReceive_log.Log("Got packet from " + ReceivedEP);
				return;
			}
			catch (SocketException ex)
			{
				if (ex.ErrorCode == 10054)
				{
					OnReceive_log.Log($"suppressing Socket Error {ex.ErrorCode}:\n" + ex.Message);
					ReceivedBuf = null; // disqualify this receive, it had an exception that should be ignored
					return;
				}
				throw;
			}
			catch
			{
				throw;
			}
		}
	}

	// an rgb2 packet object, coming from device to master
	public class RGB2Packet_DEV
	{
		public static Logger log = Logger.Main.Mine<RGB2Packet_DEV>(Debug3);
		public byte[] data;
		public byte cmd;
		public bool A;
		public RGB2Packet_DEV(byte[] data)
		{
			this.data = data;
			cmd = (byte)(data[1] & 0x0F);
			A = (data[1] & 0b10000000) != 0;
		}

		public static RGB2Packet_DEV Get(byte[] buf)
		{
			if (buf.Length >= 2)
			{
				if (buf[0] == IDs.PROTO_SIG)
					return new RGB2Packet_DEV(buf);
				log.Log("invalid packet: PROTO_SIG mismatch");
			}
			else
			{
				log.Log("invalid packet: too short");
			}
			return null;
		}
	}
}
