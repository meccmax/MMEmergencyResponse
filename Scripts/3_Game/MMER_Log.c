//------------------------------------------------------------------------------
// MM Emergency Response - logging. Goes to the script log and, on the server,
// to $profile:MMEmergency/emergency.log so intervention history survives a
// crash even if the archive write is interrupted.
//------------------------------------------------------------------------------

class MMER_Log
{
	static const int LEVEL_ERROR	= 0;
	static const int LEVEL_WARN		= 1;
	static const int LEVEL_INFO		= 2;
	static const int LEVEL_DEBUG	= 3;

	static int	s_Level		= 2;	// LEVEL_INFO - a literal, so there is no
									// static-initialisation ordering question
	static bool	s_ToFile	= true;

	static void Error(string msg)	{ Write(LEVEL_ERROR, msg); }
	static void Warn(string msg)	{ Write(LEVEL_WARN,  msg); }
	static void Info(string msg)	{ Write(LEVEL_INFO,  msg); }
	static void Debug(string msg)	{ Write(LEVEL_DEBUG, msg); }

	static void Write(int level, string msg)
	{
		if (level > s_Level)
			return;

		string prefix = "[MMER]";
		switch (level)
		{
			case LEVEL_ERROR:	prefix = "[MMER][ERROR]"; break;
			case LEVEL_WARN:	prefix = "[MMER][WARN] "; break;
			case LEVEL_INFO:	prefix = "[MMER][INFO] "; break;
			case LEVEL_DEBUG:	prefix = "[MMER][DEBUG]"; break;
		}

		string line = prefix + " " + msg;
		Print(line);

		if (!s_ToFile || !GetGame() || !GetGame().IsDedicatedServer())
			return;

		if (!FileExist(MMER_Const.PROFILE_DIR))
			MakeDirectory(MMER_Const.PROFILE_DIR);

		FileHandle fh = OpenFile(MMER_Const.LOG_PATH, FileMode.APPEND);
		if (fh != 0)
		{
			FPrintln(fh, MMER_Time.StampUTC() + " " + line);
			CloseFile(fh);
		}
	}
}
