wgcnnlib 说明


一、简况
	wgcnnlib采用c#封装，c#项目中可直接使用，用于Linux平台下视频录制，视频监控，图片检测等。

二、接口
	在namespace  WgCnnLib，class WgCnnLib下一共包含8个接口。

1、public static void OpenCamera(int cameraid)
	功能：打开摄像头。
	参数：cameraid为摄像头设备id，一般情况下从0开始编号，依次为0、1..，通常设置为0即可。
	注意事项：
	接口在整个程序中只允许运行一次。
	接口已采用多线程进行封装，使用时无需再建多线程运行。

2、 public static void RecordVideo(string filepath, string filename, int longtime, int fps)
	功能：录制视频。
	参数：
	filepath为拟存储视频路径。
	filename为拟录制视频文件名称，建议设置为×××.avi。	longtime为录制视频时间长度，单位为秒，如果想持续录制，可设置为-1。
	fps为录制视频帧率，由于树莓派性能限制，考虑还要做其他工作，建议设置不大于10帧。
	注意事项：
	接口已采用多线程进行封装，使用时无需再建多线程运行。

3、 public static ref int Detect(int delaytime)
	功能：从摄像头读取数据实时监测视角范围内有无人员存在。
	参数： delaytime为监测延迟时间，即相邻两帧检测间隔时间，单位为毫秒，取值需大于等于1，建议取500，设置越大，检测占用计算资源消耗越小。
	返回：接口返回一个int引用，该值根据检测结果实时改变，1表示有人，0表示无人。
	注意事项：
	接口在整个程序中只允许运行一次。
	接口已采用多线程进行封装，使用时无需再建多线程运行。

4、 public static void SaveFrame(string filepath,  string filename)
	功能：存储摄像头实时获取的帧。
	参数：
	filepath为拟存储图片路径。
	filename为拟储存图片文件名称，建议设置为×××.jpg。
	注意事项：
	接口已采用多线程进行封装，使用时无需再建多线程运行。

5、 public static float DetectSingle(byte[] framedata,  int height,  int width)
	功能：输入单张图片进行检测是否有人。
	参数：
	framedata 为图片数据，需要转为byte数组再传入。
	height为图片高度。
	width为图片宽度。
	返回：返回float格式数据，为图片中检测有人的最大概率值。
	注意事项：接口未采用多线程封装。

6、public static int GetFrameLen()
	功能：获取摄像头实时单帧图像二进制数据大小。
	参数：无。
	返回：返回int格式数据，为摄像头实时单帧图像二进制数据大小。
	注意事项：接口未采用多线程封装。

7、public static int GetFrame(ref byte[] framedata)
	功能：获取摄像头实时图像。
	参数：framedata为拟存入实时图像的byte数组，需要预先创建足够大的数组传入（可以采用GetFrameLen()估计，建议设置为2倍大小）。
	返回：返回int格式数据，为获取的摄像头实时单帧图像二进制数据大小；同时，图像数据会存入framedata中。
	注意事项：接口未采用多线程封装。

8、public static float GetDetectData(ref byte[] framedata, int height, int width)
	功能：检测摄像头图像或传入图像中是否有人，把检测结果显示在图像中，并返回有人的最大概率值。
	参数：
	height为图片高度。
	width为图片宽度。
	framedata为图像数据，若想检测摄像头图像，framedata需要预先创建足够大的byte数组传入（可以采用GetFrameLen()估计，建议设置为2倍大小），检测完成后，最终结果会自动存到framedata中，此时请把height 和width参数设置为0；若想传入自己图像进行检测，framedata为待检测图像byte数据（建议设置为真实数据大小的3倍），height为待检测图像高度，width为待检测图像宽度。
	返回：返回float格式数据，为图片中检测有人的最大概率值；同时，检测结果图像数据会存入framedata中。
	注意事项：接口未采用多线程封装。

三、其他说明
	2、3、4、6、7接口依赖于接口1，接口8若检测摄像头图像，也依赖于接口1，使用前请务必先运行接口1。
	接口5不依赖于其他7个接口，可根据需要使用。
	每个接口使用案例参考test.cs文件。
