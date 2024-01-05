#include "deepstream_test.h"
#include  <iostream>
#define MAX_DISPLAY_LEN 64

#define PGIE_CLASS_ID_VEHICLE 0
#define PGIE_CLASS_ID_PERSON 2

// #define LOCAL_VIDEO "/opt/nvidia/deepstream/deepstream-5.0/samples/streams/sample_720p.h264"
#define RTMP_SERVER_URL "rtmp://112.82.244.90:1935/live/vehicle_1"

/* The muxer output resolution must be set if the input streams will be of
 * different resolution. The muxer will scale all the input frames to this
 * resolution. */
#define MUXER_OUTPUT_WIDTH 1920
#define MUXER_OUTPUT_HEIGHT 1080 

/* Muxer batch formation timeout, for e.g. 40 millisec. Should ideally be set
 * based on the fastest source's framerate. */
#define MUXER_BATCH_TIMEOUT_USEC 40000

gint frame_number = 0;
// gchar pgie_classes_str[4][32] = { "Vehicle", "TwoWheeler", "Person",
//   "Roadsign"
// };

/* osd_sink_pad_buffer_probe  will extract metadata received on OSD sink pad
 * and update params for drawing rectangle, object information etc. */

static GstPadProbeReturn
osd_sink_pad_buffer_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer u_data)
{
    GstBuffer *buf = (GstBuffer *) info->data;
    guint num_rects = 0; 
    NvDsObjectMeta *obj_meta = NULL;
    guint vehicle_count = 0;
    guint person_count = 0;
    NvDsMetaList * l_frame = NULL;
    NvDsMetaList * l_obj = NULL;
    NvDsDisplayMeta *display_meta = NULL;

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta (buf);

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL; l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *) (l_frame->data);
        int offset = 0;

        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
                l_obj = l_obj->next) {
            obj_meta = (NvDsObjectMeta *) (l_obj->data);
            // std::cout<<"obj_meta的种类为"<<obj_meta->class_id;
            if (obj_meta->class_id == PGIE_CLASS_ID_VEHICLE) {
                vehicle_count++;
                num_rects++;
            }
            if (obj_meta->class_id == PGIE_CLASS_ID_PERSON) {
                person_count++;
                num_rects++;
            }
        }
        display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
        NvOSD_TextParams *txt_params  = &display_meta->text_params[0];
        display_meta->num_labels = 1;
        txt_params->display_text = (char*)g_malloc0 (MAX_DISPLAY_LEN);
        offset = snprintf(txt_params->display_text, MAX_DISPLAY_LEN, "Person = %d ", person_count);
        offset = snprintf(txt_params->display_text + offset , MAX_DISPLAY_LEN, "Vehicle = %d ", vehicle_count);

        /* Now set the offsets where the string should appear */
        txt_params->x_offset = 10;
        txt_params->y_offset = 12;

        /* Font , font-color and font-size */
        txt_params->font_params.font_name = "Serif";
        txt_params->font_params.font_size = 10;
        txt_params->font_params.font_color.red = 1.0;
        txt_params->font_params.font_color.green = 1.0;
        txt_params->font_params.font_color.blue = 1.0;
        txt_params->font_params.font_color.alpha = 1.0;

        /* Text background color */
        txt_params->set_bg_clr = 1;
        txt_params->text_bg_clr.red = 0.0;
        txt_params->text_bg_clr.green = 0.0;
        txt_params->text_bg_clr.blue = 0.0;
        txt_params->text_bg_clr.alpha = 1.0;

        nvds_add_display_meta_to_frame(frame_meta, display_meta);
    }

    // g_print ("Frame Number = %d Number of objects = %d "
    //         "Vehicle Count = %d Person Count = %d\n",
    //         frame_number, num_rects, vehicle_count, person_count);
    // frame_number++;
    return GST_PAD_PROBE_OK;
}

static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ERROR:{
      gchar *debug;
      GError *error;
      gst_message_parse_error (msg, &error, &debug);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), error->message);
      if (debug)
        g_printerr ("Error details: %s\n", debug);
      g_free (debug);
      g_error_free (error);
      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }
  return TRUE;
}


//usb推理在本地显示指令：gst-launch-1.0 v4l2src device=/dev/video0 ! 'video/x-raw, format=YUY2, width=1920, height=1080' ! nvvidconv ! 'video/x-raw(memory:NVMM), format=NV12' ! nvoverlaysink
//上传rtmp指令：gst-launch-1.0 -v v4l2src device=/dev/video0 ! videoconvert ! x264enc ! flvmux ! rtmpsink location="rtmp://server-ip/app/stream-key"
//运行的时候在build路径下的终端输入：PRE_LOAD=../utils/libmyplugins.so ./target
//使用自己的engine文件主要在于改变dstest1_pgie_config.txt
int deepstream_func()
{
  GMainLoop *loop = NULL;
  GstElement *pipeline = NULL, *source = NULL, *h264parser = NULL,
             *decoder = NULL, *streammux = NULL, *sink = NULL, 
             *pgie = NULL, *nvvidconv = NULL, *nvosd = NULL,
             *cap_filter = NULL, *cap_filter1 = NULL , *transform = NULL,
             *nvvidconv1 = NULL,*nvvidconv2 = NULL,
             *encoder=NULL , *converter=NULL ,*muxer=NULL;

  GstBus *bus = NULL;
  guint bus_watch_id;
  GstPad *osd_sink_pad = NULL;
  GstCaps *caps = NULL, *caps1 = NULL, *convertCaps = NULL;
  GstCapsFeatures *feature = NULL;

  gst_init(NULL,NULL);
  loop = g_main_loop_new (NULL, FALSE);

  // Create Pipeline element that will form a connection of other elements 
  pipeline = gst_pipeline_new ("dstest1-usb-cam-pipeline");

  /* Source element for reading from camera */
  source = gst_element_factory_make ("v4l2src", "src_elem");

  h264parser = gst_element_factory_make ("h264parse","parser");

  /* capsfilter for v4l2src */
  cap_filter1 = gst_element_factory_make("capsfilter", "src_cap_filter1");

  cap_filter = gst_element_factory_make ("capsfilter", "src_cap_filter");

  nvvidconv1 = gst_element_factory_make ("videoconvert", "nvvidconv1");

  nvvidconv2 = gst_element_factory_make ("nvvideoconvert", "nvvidconv2");

  // Use convertor to convert from NV12 to RGBA as required by nvosd 
  nvvidconv = gst_element_factory_make ("nvvideoconvert", "osd_conv");

  pgie = gst_element_factory_make ("nvinfer", "primary-nvinference-engine");

  // Create OSD to draw on the converted RGBA buffer 
  nvosd = gst_element_factory_make ("nvdsosd", "nv-onscreendisplay");

  transform = gst_element_factory_make ("nvegltransform", "nvegl-transform");

  // sink = gst_element_factory_make ("nveglglessink", "nvvideo-renderer");
  sink = gst_element_factory_make ("rtmpsink","sink");

  encoder = gst_element_factory_make ("nvv4l2h264enc", "encoder");

  muxer = gst_element_factory_make ("flvmux","muxer");

  converter = gst_element_factory_make ("nvvideoconvert", "converter");

  // Create nvstreammux instance to form batches from one or more sources. 
  streammux = gst_element_factory_make ("nvstreammux", "stream-muxer");


  if (!pipeline || !source || !h264parser || !cap_filter1 || !cap_filter 
      || !nvvidconv1 || !nvvidconv2 || !pgie  || !nvvidconv 
      || !nvosd || !transform || !sink || !encoder || !muxer || !converter || !streammux )
  {
    g_printerr ("One or more elements could not be created. Exiting.\n");
    return -1;
  }

  caps1 = gst_caps_from_string ("video/x-raw");
  caps = gst_caps_from_string ("video/x-raw(memory:NVMM), format=NV12, framerate=30/1");

  feature = gst_caps_features_new ("memory:NVMM", NULL);
  gst_caps_set_features (caps, 0, feature);
  g_object_set (G_OBJECT (cap_filter), "caps", caps, NULL);
  g_object_set (G_OBJECT (cap_filter1), "caps", caps1, NULL);

  g_object_set (G_OBJECT (source), "device", "/dev/video0", NULL);

  g_object_set (G_OBJECT (nvvidconv2), "nvbuf-memory-type", 0, NULL);

  g_object_set (G_OBJECT (pgie),"config-file-path", "/home/nano/liujian/test_websocket/utils/dstest1_pgie_config.txt", NULL);

  g_object_set (G_OBJECT (nvvidconv), "gpu-id", 0, NULL);
  g_object_set (G_OBJECT (nvvidconv), "nvbuf-memory-type", 0, NULL);

  g_object_set (G_OBJECT (nvosd), "gpu-id", 0, NULL);

  g_object_set (G_OBJECT (sink), "location", RTMP_SERVER_URL, NULL);

  g_object_set (G_OBJECT (streammux), "batch-size", 1, NULL);
  g_object_set (G_OBJECT (streammux), "width", MUXER_OUTPUT_WIDTH,
                                      "height", MUXER_OUTPUT_HEIGHT,
                                      "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC,
                                      "live-source", TRUE, NULL);    

  gst_bin_add_many (GST_BIN (pipeline), source, cap_filter1,nvvidconv1, nvvidconv2, cap_filter, NULL);
  if (!gst_element_link_many (source, cap_filter1,nvvidconv1, nvvidconv2, cap_filter, NULL)) 
  {
    g_printerr ("Elements could not be linked: 1. Exiting.\n");
    return -1;
  }

  gst_bin_add (GST_BIN(pipeline), streammux);

  //********************* sinkpad和srcpad处理 *********************
  GstPad *sinkpad, *srcpad;
  gchar pad_name_sink[16] = "sink_0";
  gchar pad_name_src[16] = "src";

  sinkpad = gst_element_get_request_pad (streammux, pad_name_sink);
  if (!sinkpad) 
  {
    g_printerr ("Streammux request sink pad failed. Exiting.\n");
    return -1;
  }

  srcpad = gst_element_get_static_pad (cap_filter, pad_name_src);
  if (!srcpad)
  {
    g_printerr ("Decoder request src pad failed. Exiting.\n");
    return -1;
  }

  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
  {
    g_printerr ("Failed to link decoder to stream muxer. Exiting.\n");
    return -1;
  }

  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);
  //********************* sinkpad和srcpad处理 *********************

  gst_bin_add_many (GST_BIN (pipeline),pgie, nvvidconv, nvosd, transform, h264parser ,converter,encoder,muxer,sink, NULL);

  if (!gst_element_link(streammux,pgie))
  {
    g_printerr ("streammux,pgie could not be linked: Exiting.\n");
    return -1;
  }

  if (!gst_element_link(pgie,nvvidconv))
  {
    g_printerr ("pgie,nvvidconv could not be linked: Exiting.\n");
    return -1;
  }

  if (!gst_element_link(nvvidconv,nvosd))
  {
    g_printerr ("nvvidconv,nvosd could not be linked: Exiting.\n");
    return -1;
  }

  if (!gst_element_link(nvosd,converter))
  {
    g_printerr ("nvosd,converter could not be linked: Exiting.\n");
    return -1;
  }

  if (!gst_element_link(converter,encoder))
  {
    g_printerr ("converter encoder could not be linked: Exiting.\n");
    return -1;
  }

  if (!gst_element_link(encoder,h264parser))
  {
    g_printerr ("encoder,h264parser could not be linked: Exiting.\n");
    return -1;
  }

  if (!gst_element_link(h264parser,muxer))
  {
    g_printerr ("h264parser,muxer could not be linked: Exiting.\n");
    return -1;
  }

  if (!gst_element_link(muxer,sink))
  {
    g_printerr ("muxer,sink could not be linked: Exiting.\n");
    return -1;
  }

  osd_sink_pad = gst_element_get_static_pad (nvosd, "sink");
  if (!osd_sink_pad)
  {g_print ("Unable to get sink pad\n");}
  gst_pad_add_probe (osd_sink_pad, GST_PAD_PROBE_TYPE_BUFFER,osd_sink_pad_buffer_probe, NULL,NULL);
  gst_object_unref (osd_sink_pad);

  //add a message handler 
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  // Wait till pipeline encounters an error or EOS 
  g_print ("Running...\n");
  g_main_loop_run (loop);

  // Out of the main loop, clean up 
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_print ("Deleting pipeline\n");

  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  return 0;
}