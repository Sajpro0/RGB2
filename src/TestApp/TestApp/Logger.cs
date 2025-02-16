using System;
using System.Collections.Generic;
using System.Data.SqlTypes;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace TestApp
{
	public class Logger
	{
		public static int DebugLevel => 3; // 0 being none, 1 being basic info, 2 intermediate info and 3 including method calls
		public static Logger Main = new Logger(null);
		static object writeLock = new object();

		public string Prefix = string.Empty;

		public LogLevel DefaultLogLevel = LogLevel.Info;

		public delegate void OnWriteLineDelegate(string text);
		public delegate void OnColorChangeDelegate(ConsoleColor clr);
		public delegate ConsoleColor OnGetColorDelegate();

		public OnWriteLineDelegate OnWriteLine = Console.WriteLine;
		public OnColorChangeDelegate OnColorChange = x => Console.ForegroundColor = x;
		public OnGetColorDelegate OnGetColor = () => Console.ForegroundColor;

		string pref => Prefix ?? string.Empty;

		public Logger(string prefix = null, LogLevel defaultLogLevel = null)
		{
			this.Prefix = prefix;
			DefaultLogLevel = defaultLogLevel ?? LogLevel.Info;
		}

		public void Begin()
		{
			var date = DateTime.Now;
			Log($"Logger.Begin() at {date.ToShortTimeString()} on {date.ToShortDateString()}");
		}

		public void Log(object msg, LogLevel level = null)
		{
			if (level == null)
			{
				level = DefaultLogLevel;
			}
			if (level == LogLevel.Debug && DebugLevel < 1 ||
				level == LogLevel.Debug2 && DebugLevel < 2 ||
				level == LogLevel.Debug3 && DebugLevel < 3)
				return; // skip if insufficient debug level

			string writePrefix = $"[{level.Name}] ";
			if (pref.Length > 0)
				writePrefix += $"[{pref}] ";
			var lines = (from x in msg.ToString().Split('\n')
						 where x.Trim().Length > 0
						 select x).ToArray();
			if (lines.Length > 0)
			{
				
				lock (writeLock)
				{
					var dt = DateTime.Now;
					string time = $"{dt.Hour:00}:{dt.Minute:00}:{dt.Second:00}.{(dt.Millisecond / 100):0}";
					writePrefix = $"[{time}] {writePrefix}";
					ConsoleColor clr = OnGetColor();
					if (level.Color != clr)
						OnColorChange(level.Color);
					OnWriteLine(writePrefix + lines[0]);
					if (lines.Length > 1)
					{
						string padding = new string(' ', writePrefix.Length);
						for (int i = 1; i < lines.Length; i++)
							OnWriteLine(padding + lines[i]);
					}
					if (level.Color != clr)
						OnColorChange(clr);
				}
			}
		}

		public static implicit operator Action<object, LogLevel>(Logger l)
		{
			return l.Log;
		}

		public Logger Mine(object me, LogLevel defLog = null)
		{
			var l = new Logger(GenPrefix(Prefix, me), defLog);
			l.OnWriteLine = x => OnWriteLine?.Invoke(x);
			Log($"new logger for [{l.pref}]", LogLevel.Debug2);
			return l;
		}

		public Logger Mine(object me, object descriptor, LogLevel defLog = null)
		{
			var l = new Logger(GenPrefix(GenPrefix(Prefix, me), descriptor), defLog);
			l.OnWriteLine = x => OnWriteLine?.Invoke(x);
			Log($"new logger for [{l.pref}]", LogLevel.Debug2);
			return l;
		}

		public Logger Mine<T>(LogLevel defLog = null) => Mine(typeof(T), defLog);
		public Logger Mine<T>(object descriptor, LogLevel defLog = null) => Mine(typeof(T), descriptor, defLog);

		string GenPrefix(string loggerPrefix, object newOwner)
		{
			if (newOwner == null)
				throw new ArgumentNullException(nameof(newOwner));
			if (loggerPrefix == null || loggerPrefix.Trim().Length == 0)
				loggerPrefix = string.Empty;
			else
				loggerPrefix += " ";
			if (newOwner is string str)
			{
				str = str.Trim();
				if (str.Length > 0)
				{
					return loggerPrefix + str;
				}
				throw new ArgumentException("Must be a non-empty string", nameof(newOwner));
			}
			else
			{
				Type t;
				if (newOwner is Type type)
					t = type;
				else
					t = newOwner.GetType();

				return $"{t.FullName}@(T{Thread.CurrentThread.ManagedThreadId}{(string.IsNullOrEmpty(Thread.CurrentThread.Name) ? string.Empty : $" \'{Thread.CurrentThread.Name}\'")})";
			}
		}
	}

	public class LogLevel
	{
		public static LogLevel
			Info = new LogLevel(ConsoleColor.Gray, "info"),
			Warn = new LogLevel(ConsoleColor.Yellow, "warn"),
			Error = new LogLevel(ConsoleColor.Red, "error"),
			Debug = new LogLevel(ConsoleColor.DarkCyan, "debug"),
			Debug2 = new LogLevel(ConsoleColor.DarkMagenta, "debug2"),
			Debug3 = new LogLevel(ConsoleColor.DarkGray, "debug3");

		public ConsoleColor Color;
		public string Name;
		public LogLevel(ConsoleColor c, string n)
		{
			Color = c;
			Name = n;
		}
	}

	public static class MainLogger
	{
		public static Logger Logger => TestApp.Logger.Main;

		public static void Log(string msg, LogLevel level = null) => Logger.Log(msg, level);
	}
}
