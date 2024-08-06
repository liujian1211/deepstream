# 1.数据传输流程

DeepStream读取实时流或本地流→GStreamer插件将视频流转换为MPEG→rtmp将MPEG的视频流传输至web浏览器→Web浏览器中使用JavaScript和HTML5将MPEG格式的视频流解码并显示在网页上

# 2.Jetson后期环境部署及程序、模型更新规划

环境更新采用打包镜像；模型更新采用网页端上传更新的方式

[Jetson 系列——jetson nano制作SD卡备份镜像以及还原_jetsonnano备份成镜像-CSDN博客](https://blog.csdn.net/weixin_42264234/article/details/119977617?spm=1001.2101.3001.6650.1&utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7ERate-1-119977617-blog-116532183.235%5Ev39%5Epc_relevant_3m_sort_dl_base4&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7ERate-1-119977617-blog-116532183.235%5Ev39%5Epc_relevant_3m_sort_dl_base4&utm_relevant_index=2)

# 3.程序更新和模型更新的本质

使用脚本将更新的库和模型替换原来的库和模型，重启生效

# 4.实施

设备：Jetson nano，IP：10.0.1.216(在/etc/network/interface中可以修改IP，修改完后重启)

锁屏密码：yahboom

root账户：root，密码：root

非root账户：nano，密码：yahboom

# 5.指令

在设备端cd至/home/nano/liujian/test_websocket/utils

运行指令`LD_PRELOAD=./libmyplugins.so ../build/target` #先预加载[libmyplugins.so](http://libmyplugins.so/)，再执行编译的target可执行文件

# 6.使用自己的模型

1）ps工作站中使用yolov5-5.0训练自己的pt模型（/home/ps/honghesong/yolov5-5.0）

2）在设备端的yolov5-5.0工程中，将pt模型转为wts模型（ **python gen_wts.py -w xx.pt -o wts_files/xx.wts**），这步也可以在上述的工作站工程中完成

3）在设备端的tensorrtx-yolov5-v5.0/yolov5工程中，新建build文件夹，并修改yololayer.h中的CLASS_NUM

4）cd build

cmake ..

make

5）上步后会在build文件夹中生成可执行文件yolov5，运行指令`sudo ./yolov5 -s manhole_0122.wts manhole_0122.engine s`，将wts文件转为engine

6）测试：运行指令`sudo ./yolov5 -d xxx.engine test_images`运行结果保存在build中

7）应用到deepstream中：

①将上述build中生成的engine和libmyplugins.so放到设备端的test_websocket/utils中，并更改该目录下的dstest1_pgie_config.txt中的model-engine-file、labelfile-path和num-detected-classes

②设备端的test_websocket/customparser_yolov5/nvdsparsebbox_Yolo.cpp更改 **NUM_CLASSES_YOLO**

③最外面的cmake中更改生成库的名字 **set(TARGET_LIB libnvdsinfer_custom_impl_Yolo_damage19)，**cmake之后即能生成该库，然后在dstest1_pgie_config.txt中的custom-lib-path字段也要更改custom-lib-path的库为新生成的库

④`LD_PRELOAD=/home/nano/liujian/test_websocket/utils/libmyplugins.so ./tcp_ip_message`开启程序，注意，一定要程序里的网址开了才可以测试，因为程序基于报文，只有报文给回复了才会运行

# 7.开机自启

1）/home/nano/startup.sh脚本为启动脚本

2）/etc/profile的最后添加 `/home/nano/startup.sh`即可在开机时启动该脚本

# 8.追踪算法

使用nvidia自带的nvdcf算法，在部署时，只需在pipeline中加入tracker这个element

`nvtracker = gst_element_factory_make("nvtracker", "tracker");` //新建`nvtracker`元素，起名“tracker”

`set_tracker_properties(GstElement *nvtracker)` //设置该追踪器的属性

配置tracker属性时，在track_config.txt中设置基本属性：

1）ll-lib-file设置追踪器类型，当前deepstream-5.0只支持libnvds_mot_iou.so（iou追踪器）、libnvds_mot_klt.so（klt追踪器）、libnvds_nvdcf.so（nvdcf追踪器），三种，性能由低到高，选择追踪器时，只需修改ll-lib-file对应的库就行；deepstream-6.0支持deepsort，届时只需更换库为libnvds_nvmultiobjecttracker.so即可

2）在其中的ll-config-file指向的yaml文件中设置追踪相关的属性，后期调整时只需调整该yaml中的参数即可

# 9.修改

由于要使tcp_ip_message.cpp和deepstream.cpp关联，因此修改了启动项，

`LD_PRELOAD=/home/nano/liujian/test_websocket/utils/libmyplugins.so ./tcp_ip_message`

上述修改就无需再生成target，而只需生成tcp_ip_message