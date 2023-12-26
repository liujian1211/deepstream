int deepstream_germen()
{
  GMainLoop *loop = NULL;
  GstElement *pipeline = NULL, *source = NULL, *h264parser = NULL,
      *decoder = NULL, *streammux = NULL, *sink = NULL, *pgie = NULL, *nvvidconv = NULL,
      *nvosd = NULL;

  GstElement *cap_filter = NULL;
  GstElement *transform = NULL;
  GstBus *bus = NULL;
  guint bus_watch_id;
  GstPad *osd_sink_pad = NULL;

  perf_measure perf_measure;

  int current_device = -1;
  cudaGetDevice(&current_device);
  struct cudaDeviceProp prop;
  cudaGetDeviceProperties(&prop, current_device);

  /* Check input arguments */
  if (argc != 2) {
    g_printerr ("Usage: %s <yml file>\n", argv[0]);
    g_printerr ("OR: %s <H264 filename>\n", argv[0]);
    g_printerr ("OR: %s camera\n", argv[0]);
    return -1;
  }
  bool USE_CAMERA_INPUT = !strcmp(argv[1], "camera"); 

  /* Standard GStreamer initialization */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  perf_measure.pre_time = GST_CLOCK_TIME_NONE;
  perf_measure.total_time = GST_CLOCK_TIME_NONE;
  perf_measure.count = 0;  

  /* Create gstreamer elements */
  // Create Pipeline element that will form a connection of other elements 
  pipeline = gst_pipeline_new ("dstest1-usb-cam-pipeline");

  // Create nvstreammux instance to form batches from one or more sources. 
  streammux = gst_element_factory_make ("nvstreammux", "stream-muxer");

  if (!pipeline || !streammux) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  if (g_str_has_suffix (argv[1], ".yml") || g_str_has_suffix (argv[1], ".yaml")) {
      nvds_parse_streammux(streammux, argv[1],"streammux");
  }

  if (USE_CAMERA_INPUT) {

    GstElement *cap_filter1 = NULL;
    GstCaps *caps = NULL, *caps1 = NULL, *convertCaps = NULL;
    GstCapsFeatures *feature = NULL;
    GstElement *nvvidconv1 = NULL;
    GstElement *nvvidconv2 = NULL;

    g_object_set (G_OBJECT (streammux), "batch-size", 1, NULL);

    g_object_set (G_OBJECT (streammux), "width", MUXER_OUTPUT_WIDTH_CAMERA, "height",
          MUXER_OUTPUT_HEIGHT_CAMERA,
          "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, "live-source", TRUE, \
          NULL);

    /* Source element for reading from camera */
    source = gst_element_factory_make ("v4l2src", "src_elem");
    if (!source  ) {
        g_printerr ("Could not create 'src_elem'.\n");
        return -1;
    }
    //Source setting 
    g_object_set (G_OBJECT (source), "device", "/dev/video0", NULL);
    
    /* capsfilter for v4l2src */
    cap_filter1 = gst_element_factory_make("capsfilter", "src_cap_filter1");
    if (!cap_filter1) {
        g_printerr ("Could not create 'src_cap_filter1'.\n");
        return -1;
    }

    caps1 = gst_caps_from_string ("video/x-raw");

    cap_filter = gst_element_factory_make ("capsfilter", "src_cap_filter");
    if (!cap_filter) {
      g_printerr ("Could not create 'src_cap_filter'.\n");
      return -1;
    }

    caps = gst_caps_from_string ("video/x-raw(memory:NVMM), format=NV12, framerate=30/1");

    if(!prop.integrated) {
      nvvidconv1 = gst_element_factory_make ("videoconvert", "nvvidconv1");
      if (!nvvidconv1) {
        g_printerr ("Failed to create 'nvvidconv1'.\n");
        return -1;
      }
    }

    feature = gst_caps_features_new ("memory:NVMM", NULL);
    gst_caps_set_features (caps, 0, feature);
    g_object_set (G_OBJECT (cap_filter), "caps", caps, NULL);
    g_object_set (G_OBJECT (cap_filter1), "caps", caps1, NULL);

    nvvidconv2 = gst_element_factory_make ("nvvideoconvert", "nvvidconv2");
    if (!nvvidconv2) {
      g_printerr ("Failed to create 'nvvidconv2'.\n");
      return -1;
    }

    g_object_set (G_OBJECT (nvvidconv2), "nvbuf-memory-type", 0, NULL);

    if(!prop.integrated) {
      gst_bin_add_many (GST_BIN (pipeline), source, cap_filter1,
          nvvidconv1, nvvidconv2, cap_filter, NULL);
      if (!gst_element_link_many (source, cap_filter1,
          nvvidconv1, nvvidconv2, cap_filter, NULL)) {
        g_printerr ("Elements could not be linked: 1. Exiting.\n");
        return -1;
      }
    } else {
      gst_bin_add_many (GST_BIN (pipeline), source, cap_filter1,
          nvvidconv2, cap_filter, NULL);
      if (!gst_element_link_many (source, cap_filter1,
          nvvidconv2, cap_filter, NULL)) {
        g_printerr ("Elements could not be linked: 1. Exiting.\n");
        return -1;
      }
    }

  } else {

    /* Source element for reading from the file */
    source = gst_element_factory_make ("filesrc", "file-source");

    /* If filename has been passed as a parameter */
    if (g_str_has_suffix (argv[1], ".h264")) {
      /* we set the input filename to the source element */
      g_object_set (G_OBJECT (source), "location", argv[1], NULL);

      g_object_set (G_OBJECT (streammux), "batch-size", 1, NULL);

      g_object_set (G_OBJECT (streammux), "width", MUXER_OUTPUT_WIDTH, "height",
          MUXER_OUTPUT_HEIGHT,
          "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, "live-source", FALSE, NULL);
    }

    /* If yaml has been passed as a parameter */
    if (g_str_has_suffix (argv[1], ".yml") || g_str_has_suffix (argv[1], ".yaml")) {
      nvds_parse_file_source(source, argv[1],"source");
    }

    /* Since the data format in the input file is elementary h264 stream,
    * we need a h264parser */
    h264parser = gst_element_factory_make ("h264parse", "h264-parser");

    /* Use nvdec_h264 for hardware accelerated decode on GPU */
    decoder = gst_element_factory_make ("nvv4l2decoder", "nvv4l2-decoder");

    if (!source || !h264parser || !decoder ) {
      g_printerr ("One element could not be created. Exiting.\n");
      return -1;
    }

    gst_bin_add_many (GST_BIN (pipeline),
        source, h264parser, decoder, NULL);
    
    /* file-source -> h264-parser -> nvh264-decoder */
    if (!gst_element_link_many (source, h264parser, decoder, NULL)) {
      g_printerr ("Elements could not be linked: 1. Exiting.\n");
      return -1;
    }

  }

  gst_bin_add (GST_BIN(pipeline), streammux);

  // Use nvinfer to run inferencing on decoder's output,
  // behaviour of inferencing is set through config file 
  pgie = gst_element_factory_make ("nvinfer", "primary-nvinference-engine");

  // Set all the necessary properties of the nvinfer element,
  //   the necessary ones are : 
  g_object_set (G_OBJECT (pgie),
        "config-file-path", "dstest1_usb_pgie_config.yml", NULL);

  // Use convertor to convert from NV12 to RGBA as required by nvosd 
  nvvidconv = gst_element_factory_make ("nvvideoconvert", "osd_conv");
  if (!nvvidconv) {
    g_printerr ("Failed to create 'osd_conv'.\n");
    return -1;
  }
  g_object_set (G_OBJECT (nvvidconv), "gpu-id", 0, NULL);
  g_object_set (G_OBJECT (nvvidconv), "nvbuf-memory-type", 0, NULL);

  // Create OSD to draw on the converted RGBA buffer 
  nvosd = gst_element_factory_make ("nvdsosd", "nv-onscreendisplay");
  if (!nvosd) {
    g_printerr ("Failed to create 'nv-onscreendisplay'.\n");
    return -1;
  }
  g_object_set (G_OBJECT (nvosd), "gpu-id", 0, NULL);

  // Finally render the osd output 
  if(prop.integrated) {
    transform = gst_element_factory_make ("nvegltransform", "nvegl-transform");
  }
  sink = gst_element_factory_make ("nveglglessink", "nvvideo-renderer");

  if (!pgie || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  if(!transform && prop.integrated) {
    g_printerr ("One tegra element could not be created. Exiting.\n");
    return -1;
  }

  GstPad *sinkpad, *srcpad;
  gchar pad_name_sink[16] = "sink_0";
  gchar pad_name_src[16] = "src";

  sinkpad = gst_element_get_request_pad (streammux, pad_name_sink);
  if (!sinkpad) {
    g_printerr ("Streammux request sink pad failed. Exiting.\n");
    return -1;
  }

  if (USE_CAMERA_INPUT) {
    srcpad = gst_element_get_static_pad (cap_filter, pad_name_src);
  } else {
    srcpad = gst_element_get_static_pad (decoder, pad_name_src);
  }
  if (!srcpad) {
    g_printerr ("Decoder request src pad failed. Exiting.\n");
    return -1;
  }

  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
      g_printerr ("Failed to link decoder to stream muxer. Exiting.\n");
      return -1;
  }
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  // Set up the pipeline 
  // we add all elements into the pipeline 
  if(prop.integrated) {
    gst_bin_add_many (GST_BIN (pipeline),
      pgie, nvvidconv, nvosd, transform, sink, NULL);

    if (!gst_element_link_many (streammux, pgie,
        nvvidconv, nvosd, transform, sink, NULL)) {
      g_printerr ("Elements could not be linked: 2. Exiting.\n");
      return -1;
    }
  }
  else {
    gst_bin_add_many (GST_BIN (pipeline),
       pgie, nvvidconv, nvosd, sink, NULL);

    if (!gst_element_link_many (streammux, pgie,
        nvvidconv, nvosd, sink, NULL)) {
      g_printerr ("Elements could not be linked: 2. Exiting.\n");
      return -1;
    }
  }

  // Lets add probe to get informed of the meta data generated, we add probe to
  // the sink pad of the osd element, since by that time, the buffer would have
  // had got all the metadata. 
  osd_sink_pad = gst_element_get_static_pad (nvosd, "sink");
  if (!osd_sink_pad)
    g_print ("Unable to get sink pad\n");
  //else
    gst_pad_add_probe (osd_sink_pad, GST_PAD_PROBE_TYPE_BUFFER,
        osd_sink_pad_buffer_probe, &perf_measure, NULL);
  gst_object_unref (osd_sink_pad);


// we add a message handler 
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  // Set the pipeline to "playing" state 
  if (USE_CAMERA_INPUT ) {
    g_print ("Using USB camera input\n");
  } else {
    g_print ("Using file: %s\n", argv[1]);
  }

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  // Wait till pipeline encounters an error or EOS 
  g_print ("Running...\n");
  g_main_loop_run (loop);


  // Out of the main loop, clean up 
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_print ("Deleting pipeline\n");

  if(perf_measure.total_time)
  {
    g_print ("Average fps %f\n",
      ((perf_measure.count-1)*1000000.0)/perf_measure.total_time);
  }
  
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  return 0;
}

int deepstream_useful_backup()
{
  GMainLoop *loop = NULL;
  GstElement *pipeline = NULL, *source = NULL, *h264parser = NULL,
             *decoder = NULL, *streammux = NULL, *sink = NULL, 
             *pgie = NULL, *nvvidconv = NULL, *nvosd = NULL,
             *cap_filter = NULL, *cap_filter1 = NULL , *transform = NULL,
             *nvvidconv1 = NULL,*nvvidconv2 = NULL;

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

  sink = gst_element_factory_make ("nveglglessink", "nvvideo-renderer");

  // Create nvstreammux instance to form batches from one or more sources. 
  streammux = gst_element_factory_make ("nvstreammux", "stream-muxer");


  if (!pipeline || !source || !cap_filter1 || !cap_filter 
      || !nvvidconv1 || !nvvidconv2 || !pgie  || !nvvidconv 
      || !nvosd || !transform || !sink || !streammux )
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

  gst_bin_add_many (GST_BIN (pipeline),pgie, nvvidconv, nvosd, transform, sink, NULL);
  if (!gst_element_link_many (streammux, pgie, nvvidconv, nvosd, transform, sink, NULL)) 
  {
    g_printerr ("Elements could not be linked: 2. Exiting.\n");
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