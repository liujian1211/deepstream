<<<<<<< HEAD
# 1.数据传输流程

DeepStream读取实时流或本地流→GStreamer插件将视频流转换为MPEG→rtmp将MPEG的视频流传输至web浏览器→Web浏览器中使用JavaScript和HTML5将MPEG格式的视频流解码并显示在网页上

# 2.Jetson后期环境部署及程序、模型更新规划

环境更新采用打包镜像；模型更新采用网页端上传更新的方式

[Jetson 系列——jetson nano制作SD卡备份镜像以及还原_jetsonnano备份成镜像-CSDN博客](https://blog.csdn.net/weixin_42264234/article/details/119977617?spm=1001.2101.3001.6650.1&utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7ERate-1-119977617-blog-116532183.235%5Ev39%5Epc_relevant_3m_sort_dl_base4&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7ERate-1-119977617-blog-116532183.235%5Ev39%5Epc_relevant_3m_sort_dl_base4&utm_relevant_index=2)

# 3.程序更新和模型更新的本质

使用脚本将更新的库和模型替换原来的库和模型，重启生效

# 4.指令

运行指令`LD_PRELOAD=./libmyplugins.so ../build/target` #先预加载[libmyplugins.so](http://libmyplugins.so/)，再执行编译的target可执行文件

# 5.修改

由于要使tcp_ip_message.cpp和deepstream.cpp关联，因此修改了启动项，

`LD_PRELOAD=/home/nano/liujian/test_websocket/utils/libmyplugins.so ./tcp_ip_message`

上述修改就无需再生成target，而只需生成tcp_ip_message
=======
# Jetson-deepstream-rtmp

jetson-deepstream
>>>>>>> 329743696f9a96baab72e2c31a85a6480e200af4
