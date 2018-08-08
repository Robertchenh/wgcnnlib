using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.IO;
using System.Net;
using System.Text;
using System.Windows.Forms;
using System.Web;
using System.Net.NetworkInformation;

namespace WgCnnLib
{

public class WgCnnLib
{   	
	[DllImport("libssdmobilenet.so", EntryPoint="camerarun", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
	static extern int camerarun(int cameraid);
	
	[DllImport("libssdmobilenet.so", EntryPoint="videorun", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
	static extern int videorun(string afilepath, string afilename, int alongtime, int afps);
	
	[DllImport("libssdmobilenet.so", EntryPoint="detectrun", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
	static extern int detectrun(ref int adata, int adelaytime);
	
	[DllImport("libssdmobilenet.so", EntryPoint="capturesaverun", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
	static extern int capturesaverun(string afilepath, string afilename);
	
	[DllImport("libssdmobilenet.so", EntryPoint="persondetectrun", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
	static extern float persondetectrun(byte[] acapturedata, int aheight, int awidth);
	
	[DllImport("libssdmobilenet.so", EntryPoint="getframerun", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
	static extern int getframerun(IntPtr aframedata);
	
	[DllImport("libssdmobilenet.so", EntryPoint="getframelenrun", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
	static extern int getframelenrun();
	
	[DllImport("libssdmobilenet.so", EntryPoint="getdetectdatarun", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
	static extern float getdetectdatarun(IntPtr aframedata, int height, int width);
	
	public static int personPresence = 0;
	
	public static float GetDetectData(ref byte[] oframedata, int height=0, int width=0)
	{
		int size = Marshal.SizeOf(oframedata[0]) * oframedata.Length;
		IntPtr postdata = Marshal.AllocHGlobal(size);
		try{
			Marshal.Copy(oframedata, 0, postdata, oframedata.Length);
			}
		finally{
			}
		
		float result = getdetectdatarun(postdata, height, width);
		
		Marshal.Copy(postdata, oframedata, 0, size);
		Marshal.FreeHGlobal(postdata);
		return result;
	}
	
	
	
	public static int GetFrameLen()
	{
		int result = getframelenrun();
		return result;
	}
	
	
	
	
	public static int GetFrame(ref byte[] oframedata)
	{
		int size = Marshal.SizeOf(oframedata[0]) * oframedata.Length;
		IntPtr postdata = Marshal.AllocHGlobal(size);
		try{
			Marshal.Copy(oframedata, 0, postdata, oframedata.Length);
			}
		finally{
			}
		
		int oframelen = getframerun(postdata);
		
		Marshal.Copy(postdata, oframedata, 0, size);
		Marshal.FreeHGlobal(postdata);
		return oframelen;
	}
	
	public static void OpenCamera(int cameraid)
	{
		Task.Run(async () =>{
				await Task.Delay(1);
				camerarun(cameraid);	    
			}
		);
	}
	
	public static void RecordVideo(string ofilepath, string ofilename, int olongtime, int ofps)
	{
		Task.Run(async () =>{
				await Task.Delay(1);
				videorun(ofilepath, ofilename, olongtime, ofps);	    
			}
		);
	}
	
	public static ref int Detect(int odelaytime)
	{
		Task.Run(async () =>{
				await Task.Delay(1);
				detectrun(ref personPresence, odelaytime);    
			}
		);
		return ref personPresence;
	}
	
	public static void SaveFrame(string ofilepath, string ofilename)
	{
		Task.Run(async () =>{
				await Task.Delay(1);
				capturesaverun(ofilepath, ofilename);	    
			}
		);
	}
	
	public static float DetectSingle(byte[] ocapturedata, int oheight, int owidth)
	{
		//int size = Marshal.SizeOf(ocapturedata[0]) * ocapturedata.Length;
		//IntPtr postdata = Marshal.AllocHGlobal(size);
		//try{
			//Marshal.Copy(ocapturedata, 0, postdata, ocapturedata.Length);
			//}
		//finally{
			//}
		float maxprobability = persondetectrun(ocapturedata, oheight, owidth);	    
		return maxprobability;
	}
	
}

}
