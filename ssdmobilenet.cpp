// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2017 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdio.h>
#include <algorithm>
#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>

#include "net.h"
#include "cpu.h"
#include "benchmark.h"


#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>

#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <utility>
typedef unsigned char BYTE;

using namespace std;

void *videofun(void *bvideopara);
void *detectfun(void *bdetectpara);
void *camerafun(int bcameraid);



static cv::Mat globalframe;
pthread_mutex_t mutex;




ncnn::Net mobile_net;
ncnn::Net mobile_netscd;
bool loaddata;
bool loaddatascd;



struct Object{
    cv::Rect rec;
    int class_id;
    float prob;
};

struct video_para{
	char *filepath;
	char *filename;
	int longtime;
	int fps;
};

struct detect_para{
	int *data;
	int delaytime;
};

const char* class_names[] = {"background",
                            "aeroplane", "bicycle", "bird", "boat",
                            "bottle", "bus", "car", "cat", "chair",
                            "cow", "diningtable", "dog", "horse",
                            "motorbike", "person", "pottedplant",
                            "sheep", "sofa", "train", "tvmonitor"};

cv::Mat detect_mobilenet(ncnn::Net& mobilenet, cv::Mat& raw_img, float show_threshold)
{
    int img_h = raw_img.size().height;
    int img_w = raw_img.size().width;
    int input_size = 300;
    ncnn::Mat in = ncnn::Mat::from_pixels_resize(raw_img.data, ncnn::Mat::PIXEL_BGR, raw_img.cols, raw_img.rows, input_size, input_size);
    const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
    const float norm_vals[3] = {1.0/127.5,1.0/127.5,1.0/127.5};
    in.substract_mean_normalize(mean_vals, norm_vals);
    ncnn::Mat out;
    ncnn::Extractor ex = mobilenet.create_extractor();
    ex.set_light_mode(true);
    ex.input("data", in);
    ex.extract("detection_out",out);

    std::vector<Object> objects;
    for (int iw=0;iw<out.h;iw++)
    {
        Object object;
        const float *values = out.row(iw);
        object.class_id = values[0];
        object.prob = values[1];
        object.rec.x = values[2] * img_w;
        object.rec.y = values[3] * img_h;
        object.rec.width = values[4] * img_w - object.rec.x;
        object.rec.height = values[5] * img_h - object.rec.y;
        objects.push_back(object);
    }

    for(int i = 0;i<objects.size();++i)
    {
        Object object = objects.at(i);
        if(object.prob > show_threshold)
        {
            cv::rectangle(raw_img, object.rec, cv::Scalar(255, 0, 0));
            std::ostringstream pro_str;
            pro_str<<object.prob;
            std::string label = std::string(class_names[object.class_id]) + ": " + pro_str.str();
            int baseLine = 0;
            cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
            cv::rectangle(raw_img, cv::Rect(cv::Point(object.rec.x, object.rec.y- label_size.height),
                                  cv::Size(label_size.width, label_size.height + baseLine)),
                      cv::Scalar(255, 255, 255), CV_FILLED);
            cv::putText(raw_img, label, cv::Point(object.rec.x, object.rec.y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
        }
    }
    
    return raw_img;
}




void *detectfun(void *cdetctpara)
{
	//pthread_mutex_init(&mutex, NULL);
	detect_para *ddetctpara;
	ddetctpara = (detect_para *)cdetctpara;
	ncnn::Net mobilenet;
	mobilenet.load_param("mobilenet_ssd_voc_ncnn.param");
	mobilenet.load_model("mobilenet_ssd_voc_ncnn.bin");
	ncnn::set_cpu_powersave(0);
	double start, end;
	bool have = false;
	
	pthread_mutex_lock(&mutex);
	cv::Mat frame_old = globalframe.clone();
	pthread_mutex_unlock(&mutex);
	usleep(1000);
	while(frame_old.empty()){
		printf("Camera is not opened, waiting for a few secs!\n");
		pthread_mutex_lock(&mutex);
		frame_old = globalframe.clone();
		pthread_mutex_unlock(&mutex);
		usleep(1000000);
	}
	
	cv::Mat frame_new;
	int img_h = frame_old.size().height;
	int img_w = frame_old.size().width;
	
	int *ddata = ddetctpara->data;
	int ddelaytime = ddetctpara->delaytime;
	
	for(;;){
		have = false;
		pthread_mutex_lock(&mutex);
		frame_old = globalframe.clone();
		pthread_mutex_unlock(&mutex);
		usleep(1000);
		start = ncnn::get_current_time();
		cv::Mat& raw_img = frame_old;
		int input_size = 300;
		ncnn::Mat in = ncnn::Mat::from_pixels_resize(raw_img.data, ncnn::Mat::PIXEL_BGR, raw_img.cols, raw_img.rows, input_size, input_size);
		const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
		const float norm_vals[3] = {1.0/127.5,1.0/127.5,1.0/127.5};
		in.substract_mean_normalize(mean_vals, norm_vals);
		float show_threshold = 0.6;
		ncnn::Mat out;

		ncnn::Extractor ex = mobilenet.create_extractor();
		ex.set_light_mode(true);

		ex.input("data", in);
		ex.extract("detection_out",out);

		std::vector<Object> objects;
		   
		for (int iw = 0; iw < out.h; iw++){
		   Object object;
		   const float *values = out.row(iw);
		   object.class_id = values[0];
		   object.prob = values[1];
		   object.rec.x = values[2] * img_w;
		   object.rec.y = values[3] * img_h;
		   object.rec.width = values[4] * img_w - object.rec.x;
		   object.rec.height = values[5] * img_h - object.rec.y;
		   objects.push_back(object);
		}

		for(int i = 0;i<objects.size();++i){
		   Object object = objects.at(i);
		   if(object.prob > show_threshold){
			   
			   if(object.class_id == 15){
					have = true;
				}
				
				cv::rectangle(raw_img, object.rec, cv::Scalar(255, 0, 0));
				std::ostringstream pro_str;
				pro_str<<object.prob;
				std::string label = std::string(class_names[object.class_id]) + ": " + pro_str.str();
				int baseLine = 0;
				cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
				cv::rectangle(raw_img, cv::Rect(cv::Point(object.rec.x, object.rec.y- label_size.height),
								  cv::Size(label_size.width, label_size.height + baseLine)),
					  cv::Scalar(255, 255, 255), CV_FILLED);
				cv::putText(raw_img, label, cv::Point(object.rec.x, object.rec.y),
					cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
		   }
		}

		if(have)
			*ddata=1;
		else
			*ddata = 0;
		frame_new = raw_img;  
		cv::imshow("result",frame_new);
		end = ncnn::get_current_time();
		float times = end - start;
		printf("this frame detect time is:   %f ms\n" , times);
		cv::waitKey(ddelaytime); 
	}
}

void *videofun(void *cvideopara)
{
	//pthread_mutex_init(&mutex, NULL);
	video_para *dvideopara;
	dvideopara = (video_para *)cvideopara;
	string dfilepath = dvideopara->filepath;
	string dfilename = dvideopara->filename;
	int dlongtime = dvideopara->longtime;
	int dfps = dvideopara->fps;
	if (opendir(dvideopara->filepath) == NULL){
		printf("there is no videos path, creat one!\n");
		mkdir(dvideopara->filepath, 00700);
	}
	else{
		printf("videos path  is exist!\n");
	}
	string plus = "/";
	string outputvideopath = dfilepath + plus + dfilename;
	cout<<"video path is: "<<outputvideopath<<endl;
	pthread_mutex_lock(&mutex);
    cv::Mat video_frame = globalframe.clone();
    pthread_mutex_unlock(&mutex);
	usleep(1000);
	while(video_frame.empty()){
		printf("camera is not going, waiting for few second!\n");
		pthread_mutex_lock(&mutex);
		video_frame = globalframe.clone();
		pthread_mutex_unlock(&mutex);
		usleep(1000000);
	}
	
	int img_h = video_frame.size().height;
    int img_w = video_frame.size().width;
    cv::Size videosize(img_w, img_h);
	cv::VideoWriter outputvideo(outputvideopath, CV_FOURCC('M', 'J', 'P', 'G'), dfps, videosize, 1);
	double start = ncnn::get_current_time();
	double end = ncnn::get_current_time();
	int testtime = 0;
	if (dlongtime == -1){
		int id = 1;
		int interval = (int)(1000 / dfps);
		int scd = 0;
		int a,b,c;
		while(id){
			scd = (int)(id / dfps);
			a = (int)(id - scd * dfps -1) * interval;
			b = (int)((end - start)- (scd * 1000));
			c = (int)(id - scd * dfps) * interval;
			
			if(((b >= a)&&(b <= c)) || (b >= c)){
				pthread_mutex_lock(&mutex);
				video_frame = globalframe.clone();
				pthread_mutex_unlock(&mutex);
				outputvideo << video_frame;	
				id++;
				
				if(testtime <= (int)((end - start)/1000) - 2){
					printf("video is going, time of duration is: %d s, the frameid is: %d \n", (int)((end - start)/1000), id-1);
					testtime = (int)((end - start)/1000);
				}
			}
		
			end = ncnn::get_current_time();
			usleep(1000);
		}
	}
	else{	
		int lenframe = dlongtime * dfps;
		int id = 1;
		int interval = (int)(1000 / dfps);
		int scd = 0;
		int a,b,c;
		while(id < lenframe + 1){
			scd = (int)(id / dfps);
			a = (int)(id - scd * dfps -1) * interval;
			b = (int)((end - start)- (scd * 1000));
			c = (int)(id - scd * dfps) * interval;
			
			if(((b >= a)&&(b <= c)) || (b >= c)){
				pthread_mutex_lock(&mutex);
				video_frame = globalframe.clone();
				pthread_mutex_unlock(&mutex);
				outputvideo << video_frame;	
				id++;
				
				if(testtime <= (int)((end - start)/1000) - 2){
					printf("video is going, time of duration is: %d s, the frameid is: %d \n", (int)((end - start)/1000), id-1);
					testtime = (int)((end - start)/1000);
				}
			}
		
			end = ncnn::get_current_time();
			usleep(1000);
		}
		
		
	}
	return 0;
}

void *camerafun(int bcameraid)
{
	if(!globalframe.empty()){
		printf("camera already going!\n");
		return 0;
	}
	
	pthread_mutex_init(&mutex, NULL);
	cv::VideoCapture cap(bcameraid);
	if(!cap.isOpened()){
       printf("camera initial failed!\n");
       return 0; 
    }
    else{
		printf("camera initial succeed!\n");
	}
	double start = ncnn::get_current_time();
	double end = ncnn::get_current_time();
	int testtime = 0;
	while(true){
		pthread_mutex_lock(&mutex);
		cap>>globalframe;
		pthread_mutex_unlock(&mutex);
		if(testtime <= int((end - start)/1000) - 2){
			printf("camera is going!\n");
			testtime = int((end - start)/1000);
		}
		end = ncnn::get_current_time();
		usleep(1000);
	}
}


void capturesavefun(char *bfilepath, char *bfilename)
{
	string dfilepath = bfilepath;
	string dfilename = bfilename;
	if (opendir(bfilepath) == NULL){
		printf("there is no capture path, creat one!\n");
		mkdir(bfilepath, 00700);
	}
	else{
		printf("capture path  is exist!\n");
	}
	string plus = "/";
	string outputcapturepath = dfilepath + plus + dfilename;
	pthread_mutex_lock(&mutex);
    cv::Mat capture_frame = globalframe.clone();
    pthread_mutex_unlock(&mutex);
	usleep(1000);
	while(capture_frame.empty()){
		printf("camera is not going, waiting for few second!\n");
		pthread_mutex_lock(&mutex);
		capture_frame = globalframe.clone();
		pthread_mutex_unlock(&mutex);
		usleep(1000000);
	}
	cv::imwrite(outputcapturepath, capture_frame);
	cout<<"capture path is: "<<outputcapturepath<<", save succeed!"<<endl;
}


float persondetectfun(unsigned char *capturedata, int height, int width)
{
	cv::Mat frame_old = cv::imdecode(cv::Mat(height, width, CV_8UC3, capturedata), CV_LOAD_IMAGE_UNCHANGED);
	if(!loaddata){
		printf("load model data......\n");
		mobile_net.load_param("mobilenet_ssd_voc_ncnn.param");
		mobile_net.load_model("mobilenet_ssd_voc_ncnn.bin");
		loaddata = true;
	}
	ncnn::set_cpu_powersave(0);
	double start, end;
	float result = 0;
	//cv::Mat frame_new;
	int img_h = frame_old.size().height;
	int img_w = frame_old.size().width;
	start = ncnn::get_current_time();
	cv::Mat& raw_img = frame_old;
	int input_size = 300;
	ncnn::Mat in = ncnn::Mat::from_pixels_resize(raw_img.data, ncnn::Mat::PIXEL_BGR, raw_img.cols, raw_img.rows, input_size, input_size);
	const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
	const float norm_vals[3] = {1.0/127.5,1.0/127.5,1.0/127.5};
	in.substract_mean_normalize(mean_vals, norm_vals);
	float show_threshold = 0.6;
	ncnn::Mat out;

	ncnn::Extractor ex = mobile_net.create_extractor();
	ex.set_light_mode(true);

	ex.input("data", in);
	ex.extract("detection_out",out);

	std::vector<Object> objects;
	   
	for (int iw = 0; iw < out.h; iw++){
	   Object object;
	   const float *values = out.row(iw);
	   object.class_id = values[0];
	   object.prob = values[1];
	   object.rec.x = values[2] * img_w;
	   object.rec.y = values[3] * img_h;
	   object.rec.width = values[4] * img_w - object.rec.x;
	   object.rec.height = values[5] * img_h - object.rec.y;
	   objects.push_back(object);
	}

	for(int i = 0;i<objects.size();++i){
	   Object object = objects.at(i);
	   if(object.prob > show_threshold){
		   
		   if(object.class_id == 15){
				if(object.prob > result){
					result = object.prob;
				}
			}
			
			//cv::rectangle(raw_img, object.rec, cv::Scalar(255, 0, 0));
			//std::ostringstream pro_str;
			//pro_str<<object.prob;
			//std::string label = std::string(class_names[object.class_id]) + ": " + pro_str.str();
			//int baseLine = 0;
			//cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
			//cv::rectangle(raw_img, cv::Rect(cv::Point(object.rec.x, object.rec.y- label_size.height),
							  //cv::Size(label_size.width, label_size.height + baseLine)),
				  //cv::Scalar(255, 255, 255), CV_FILLED);
			//cv::putText(raw_img, label, cv::Point(object.rec.x, object.rec.y),
				//cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
	   }
	}

	//frame_new = raw_img;  
	//cv::imshow("result",frame_new);
	//end = ncnn::get_current_time();
	//float times = end - start;
	//printf("this frame detect time is:   %f ms\n" , times);
	//cv::waitKey(0); 
	return result;
	
}

int getframelenfun()
{
	pthread_mutex_lock(&mutex);
	cv::Mat capture_frame = globalframe.clone();
	pthread_mutex_unlock(&mutex);
	while(capture_frame.empty()){
		printf("camera is not going, waiting for few second!\n");
		pthread_mutex_lock(&mutex);
		capture_frame = globalframe.clone();
		pthread_mutex_unlock(&mutex);
		usleep(1000000);
	}
	vector<BYTE> dataencode;
	cv::imencode(".jpg", capture_frame, dataencode);
	return (int)dataencode.size();
}

int getframefun(BYTE *cframedata)
{
	pthread_mutex_lock(&mutex);
	cv::Mat capture_frame = globalframe.clone();
	pthread_mutex_unlock(&mutex);
	while(capture_frame.empty()){
		printf("camera is not going, waiting for few second!\n");
		pthread_mutex_lock(&mutex);
		capture_frame = globalframe.clone();
		pthread_mutex_unlock(&mutex);
		usleep(1000000);
	}
	vector<BYTE> dataencode;
	cv::imencode(".jpg", capture_frame, dataencode);
	memcpy(cframedata, dataencode.data(), (int)dataencode.size());
	
	return (int)dataencode.size();
}


float getdetectdatafun(BYTE *cframedata, int height, int width)
{
	cv::Mat frame_old;
	if((height != 0) && (width != 0)){
		frame_old = cv::imdecode(cv::Mat(height, width, CV_8UC3, cframedata), CV_LOAD_IMAGE_UNCHANGED);
	}
	else{
		pthread_mutex_lock(&mutex);
		frame_old = globalframe.clone();
		pthread_mutex_unlock(&mutex);
		usleep(1000);
		while(frame_old.empty()){
			printf("Camera is not opened, waiting for a few secs!\n");
			pthread_mutex_lock(&mutex);
			frame_old = globalframe.clone();
			pthread_mutex_unlock(&mutex);
			usleep(1000000);
		}
	}

	if(!loaddatascd){
		printf("load model data......\n");
		mobile_netscd.load_param("mobilenet_ssd_voc_ncnn.param");
		mobile_netscd.load_model("mobilenet_ssd_voc_ncnn.bin");
		loaddatascd = true;
	}
	ncnn::set_cpu_powersave(0);
	double start, end;
	float result = 0;
	//cv::Mat frame_new;
	int img_h = frame_old.size().height;
	int img_w = frame_old.size().width;
	start = ncnn::get_current_time();
	cv::Mat& raw_img = frame_old;
	int input_size = 300;
	ncnn::Mat in = ncnn::Mat::from_pixels_resize(raw_img.data, ncnn::Mat::PIXEL_BGR, raw_img.cols, raw_img.rows, input_size, input_size);
	const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
	const float norm_vals[3] = {1.0/127.5,1.0/127.5,1.0/127.5};
	in.substract_mean_normalize(mean_vals, norm_vals);
	float show_threshold = 0.6;
	ncnn::Mat out;

	ncnn::Extractor ex = mobile_netscd.create_extractor();
	ex.set_light_mode(true);

	ex.input("data", in);
	ex.extract("detection_out",out);

	std::vector<Object> objects;
	   
	for (int iw = 0; iw < out.h; iw++){
	   Object object;
	   const float *values = out.row(iw);
	   object.class_id = values[0];
	   object.prob = values[1];
	   object.rec.x = values[2] * img_w;
	   object.rec.y = values[3] * img_h;
	   object.rec.width = values[4] * img_w - object.rec.x;
	   object.rec.height = values[5] * img_h - object.rec.y;
	   objects.push_back(object);
	}

	for(int i = 0;i<objects.size();++i){
	   Object object = objects.at(i);
	   if(object.prob > show_threshold){
		   
		   if(object.class_id == 15){
				if(object.prob > result){
					result = object.prob;
				}
			}
			
			cv::rectangle(raw_img, object.rec, cv::Scalar(255, 0, 0));
			std::ostringstream pro_str;
			pro_str<<object.prob;
			std::string label = std::string(class_names[object.class_id]) + ": " + pro_str.str();
			int baseLine = 0;
			cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
			cv::rectangle(raw_img, cv::Rect(cv::Point(object.rec.x, object.rec.y- label_size.height),
							  cv::Size(label_size.width, label_size.height + baseLine)),
				  cv::Scalar(255, 255, 255), CV_FILLED);
			cv::putText(raw_img, label, cv::Point(object.rec.x, object.rec.y),
				cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
	   }
	}

	//frame_new = raw_img;  
	//cv::imshow("result",frame_new);
	//end = ncnn::get_current_time();
	//float times = end - start;
	//printf("this frame detect time is:   %f ms\n" , times);
	//cv::waitKey(0); 
	
	vector<BYTE> dataencode;
	cv::imencode(".jpg", raw_img, dataencode);
	memcpy(cframedata, dataencode.data(), (int)dataencode.size());
	
	return result;
	
}

extern "C" float getdetectdatarun(BYTE *bframedata, int height, int width)
{
	
	float result = getdetectdatafun(bframedata, height, width);
    return result;
}







extern "C" int getframelenrun()
{
	int result = getframelenfun();
    return result;
}

extern "C" int getframerun(BYTE *bframedata)
{
	
	int framelen = getframefun(bframedata);
    return framelen;
}





extern "C" float persondetectrun(BYTE *bcapturedata, int bheight, int bwidth)
{
	float result = persondetectfun(bcapturedata, bheight, bwidth);
    return result;
}



extern "C" int capturesaverun(char *bfilepath, char *bfilename)
{
	capturesavefun(bfilepath, bfilename);
    return 1;
}


extern "C" int videorun(char *bfilepath, char *bfilename, int blongtime, int bfps)
{
	video_para videopara;
	videopara.filepath = bfilepath;
	videopara.filename = bfilename;
	videopara.longtime = blongtime;
	videopara.fps = bfps;
	//pthread_t video_threadid;
	//int ret = pthread_create(&video_threadid, NULL, videofun, &videopara);
	//if(ret !=0){
		//printf("create video thread failed!\n");
		//return 0;
	//}
	//pthread_join(video_threadid, NULL);
	videofun(&videopara);
    return 1;
}

extern "C" int detectrun(int* bdata, int bdelaytime)
{
	detect_para detectpara;
	detectpara.data = bdata;
	detectpara.delaytime = bdelaytime;
	//pthread_t detect_threadid;
	//int ret = pthread_create(&detect_threadid, NULL, detectfun, &detectpara);
	//if(ret !=0){
		//printf("create frame detect thread failed!\n");
		//return 0;
	//}
	//pthread_join(detect_threadid, NULL);
	detectfun(&detectpara);
    return 1;
}

extern "C" int camerarun(int bcameraid)
{
	//int null = 1;
	//pthread_t camera_threadid;
	//int ret = pthread_create(&camera_threadid, NULL, camerafun, &null);
	//if(ret !=0){
		//printf("create camera thread failed!\n");
		//return 0;
	//}
	//pthread_join(camera_threadid, NULL);
	camerafun(bcameraid);
    return 1;
}

int main()
{
	printf("+++++++++++++++++++++++++++++++++++++++\n");
	int bdata = 0;
	int bdelaytime = 500;
	int cameraid = 0;
	//char bfilepath_= "/home/pi/ncnn/examples/ssd/build/videospath";
	//char bfile_name = "test.avi";
	//int blongtime = 30;
	//camerarun();
	//pthread_t camera_threadid;
	//int ret = pthread_create(&camera_threadid, NULL, camerafun, cameraid);
	//if(ret !=0){
		//printf("create camera thread failed!\n");
		//return 0;
	//}
	//pthread_join(camera_threadid, NULL);
	//usleep(2000000);
	//usleep(2000000);
	//printf("detect run!\n");
	//detectrun(&bdata, bdelaytime);
	//videorun(char *bfilepath, char *bfilename, int blongtime)
	//printf("+++++++++++++++++++++++++++++++++++++++++__________________________\n");
	//vector<BYTE> frametest = getframefun();
	
	//cv::Mat frame1 = cv::imdecode(cv::Mat(480, 640, CV_8UC3, frametest.data()), CV_LOAD_IMAGE_UNCHANGED);
	//printf("+++++++++++++++++++++++++++++++++++++++++over\n");
	////cout<<frametest.data()<<endl;
	//cv::imshow("test111", frame1);
	
	//cv::waitKey(0);
	
	//cv::VideoCapture cap(0);

	//cap>>globalframe;
	
	//BYTE *a = getframefun();
	//cv::Mat frame1 = cv::imdecode(cv::Mat(480, 640, CV_8UC3, a), CV_LOAD_IMAGE_UNCHANGED);
	//printf("+++++++++++++++++++++++++++++++++++++++++over\n");
	////cout<<frametest.data()<<endl;
	//cv::imshow("test111", frame1);
	//cv::waitKey(0);
	return 1;
}



