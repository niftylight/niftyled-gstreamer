/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2011-2014 Daniel Hiepler <daniel@niftylight.de>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-niftyled
 *
 * FIXME:Describe niftyled here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! niftyled ! fakesink silent=true
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <niftyled.h>
#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "gstniftyled.h"

#ifndef PACKAGE
#define PACKAGE "gst-plugins-niftyled"
#endif



GST_DEBUG_CATEGORY_STATIC(gst_niftyled_debug);
#define GST_CAT_DEFAULT gst_niftyled_debug

/* Filter signals and args */
enum
{
        /* FILL ME */
        LAST_SIGNAL
};

/* gstreamer plugin properties */
enum
{
        PROP_0,
        PROP_LOGLEVEL,
        PROP_CONFIG,
};

/* 
 * the capabilities of the inputs and outputs.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE 
(
        "sink",
        GST_PAD_SINK,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS ("ANY")
);

GST_BOILERPLATE (GstNiftyled, gst_niftyled, GstBaseSink, GST_TYPE_BASE_SINK);


/** print a line with all valid logleves */
static void _print_niftyled_loglevels()
{
        /* print loglevels */
	printf("Valid loglevels:\n\t");
	NftLoglevel i;
	for(i = L_MAX+1; i<L_MIN-1; i++)
		printf("%s ", nft_log_level_to_string(i));
        printf("\n");
}



/******************************************************************************/



/**
 * pipeline started
 */
static gboolean gst_niftyled_start(GstBaseSink *bsink)
{
        GstNiftyled *nl = GST_NIFTYLED(bsink);

        /* parse config-file */
    	LedPrefsNode *pnode;
    	if(!(pnode = led_prefs_node_from_file(nl->configfile)))
    	{
                GST_ERROR_OBJECT(bsink, "failed to load config from \"%s\"", nl->configfile);
                return false;
		}

		/* create setup from prefs-node */
    	if(!(nl->setup = led_prefs_setup_from_node(nl->prefs, pnode)))
    	{
				GST_ERROR_OBJECT(bsink, "No valid setup found in preferences file \"%s\"", nl->configfile);
				led_prefs_node_free(pnode);
				return false;
		}

		led_prefs_node_free(pnode);
		
        /* get first toplevel tile & hardware */
		LedHardware *hw;
        if(!(hw = led_setup_get_hardware(nl->setup)))
        {
                GST_ERROR_OBJECT(bsink, "no hardware found in \"%s\"", nl->configfile);
                return false;
        }
        
                
        /* determine width of input-frames - dimensions of mapped chain */
        LedFrameCord width, height;
		if(!led_setup_get_dim(nl->setup, &width, &height))
				return false;        
                
        /* amount of channels in completly mapped chain */
        //~ NftFrameChannel channels;
        //~ channels = led_chain_max_channel(nl->out_chain)+1;
                         
        /* amount of bits per channel */
        //~ LedGreyscaleWidth bitwidth;
        //~ if(!(bitwidth = led_hardware_get_greyscale_bitwidth_max(nl->first_hw)))
        //~ {
                //~ GST_ERROR_OBJECT(bsink, "failed to get maximum bitwidth");
                //~ return false;
        //~ }
       
		/* allocate frame (where our pixelbuffer resides) */
		NFT_LOG(L_INFO, "Allocating %s frame (%dx%d)", 
	        nl->format, width, height);

		LedPixelFormat *format  = led_pixel_format_from_string(nl->format);
		
        if(!(nl->frame = led_frame_new(width, height, format)))
        {
                GST_ERROR_OBJECT(bsink, "failed to create frame");
				return false;
        }

		/* do mapping */
        if(!led_hardware_list_refresh_mapping(hw))
                return false;
        /* precalc memory offsets for actual mapping */
        if(!led_chain_map_from_frame(led_hardware_get_chain(hw), nl->frame))
                return false;
		
        /* set correct gain to hardware */
        if(!led_hardware_list_refresh_gain(hw))
        {
                GST_ERROR_OBJECT(bsink, "failed to set gain");
                return false;
        }
		
        return true;
}


/**
 * pipeline stopped
 */
static gboolean gst_niftyled_stop(GstBaseSink *bsink)
{
        GstNiftyled *nl = GST_NIFTYLED(bsink);
        
        /* deinitialize hardware */
	led_hardware_list_destroy(led_setup_get_hardware(nl->setup));
	
	/* free frame */
	led_frame_destroy(nl->frame);
        
        /* free config file */
        led_prefs_deinit(nl->prefs);

        /* deinitialize pixel-format */
        led_pixel_format_destroy();
        
        return true;
}


/**
 * render a bufferful
 */
static GstFlowReturn gst_niftyled_render(GstBaseSink *bsink, GstBuffer *buffer)
{
        GstNiftyled *nl;
        if(!(nl = GST_NIFTYLED(bsink)))
                NFT_LOG_NULL(GST_FLOW_ERROR);

        /* set framebuffer */
        led_frame_set_buffer(nl->frame, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer), NULL);

        /* set endianess (flag will be changed when conversion occurs) */
        led_frame_set_big_endian(nl->frame, nl->big_endian);
        
        /* map from frame */
        LedHardware *h;
        for(h = led_setup_get_hardware(nl->setup); h; h = led_hardware_list_get_next(h))
        {
                if(!led_chain_fill_from_frame(led_hardware_get_chain(h), nl->frame))
                {
                        NFT_LOG(L_ERROR, "Error while mapping frame");
                        return GST_FLOW_ERROR;
                }
        }

        /* send frame to hardware(s) */
        led_hardware_list_send(led_setup_get_hardware(nl->setup));

        /* latch hardware */
        led_hardware_list_show(led_setup_get_hardware(nl->setup));

        
        return GST_FLOW_OK;
}


/**
 * set gStreamer property to plugin
 */
static void gst_niftyled_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
        GstNiftyled *nl = GST_NIFTYLED (object);

        
        switch(prop_id) 
        {
                case PROP_LOGLEVEL:
                {
                        if(!nft_log_level_set(nft_log_level_from_string(g_value_get_string(value))))
                        {
                                _print_niftyled_loglevels();
                                return;
                        }
                        break;
                }
                
                case PROP_CONFIG:
                {
                        const char *configfile;
                        if(!(configfile = g_value_get_string(value)))
                                return;
                        
                        strncpy(nl->configfile, configfile, sizeof(nl->configfile));
                        break;
                }
                        
                default:
                {
                        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
                }
                break;
        }
}


/**
 * get gStreamer property from plugin
 */
static void gst_niftyled_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
        GstNiftyled *nl = GST_NIFTYLED(object);

        
        switch (prop_id) 
        {
                case PROP_CONFIG:
                {
                        g_value_set_string(value, nl->configfile);
                        break;
                }

                case PROP_LOGLEVEL:
                {
                        g_value_set_string(value, nft_log_level_to_string(nft_log_level_get()));
                        break;
                }
                        
                default:
                {
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                        break;
                }
        }
}


/**
 * this function handles the link with other elements 
 *
 * @todo decent color-format selection support
 */
static gboolean gst_niftyled_set_caps (GstBaseSink *bsink, GstCaps * caps)
{
        GstNiftyled *nl = GST_NIFTYLED(bsink);

        GST_DEBUG_OBJECT(nl, "caps: %" GST_PTR_FORMAT, caps);

        /* get caps */
        GstStructure *s = gst_caps_get_structure(caps, 0);

		/* get frame dimensions */
		LedFrameCord f_width, f_height;
		if(!led_frame_get_dim(nl->frame, &f_width, &f_height))
				return false;
		
        /* check width */
        gint width;
        if(gst_structure_get_int(s, "width", &width))
        {
                if(width != (gint) f_width)
                {
                        NFT_LOG(L_ERROR, "Video width %d differs from LED-setup width %d",
                                width, f_width);
                        return false;
                }
        }

        /* check height */
        gint height;
        if(gst_structure_get_int(s, "height", &height))
        {
                if(height != (gint) f_height)
                {
                        NFT_LOG(L_ERROR, "Video height %d differs from LED-setup height %d",
                                height, f_height);
                        return false;
                }
        }

        /* get endianess */
        gint endianness;
        if(gst_structure_get_int(s, "endianness", &endianness))
                nl->big_endian = (endianness != 4321);

        /* find colorspace */
        char *colorspace;

        /* find component type */
        char *type;
        int depth;
        if(gst_structure_get_int(s, "depth", &depth))
        {
                switch(depth)
                {
                        case 24:
                        {
                                type = " u8";
                                break;
                        }

                        case 48:
                        {
                                type = " u16";
                                break;
                        }
                }
        }

        /* compose format string */
        //strncpy(nl->format, colorspace, sizeof(nl->format));
        //strncat(nl->format, type, sizeof(nl->format));



        /* we can still fail later */
        return true;
}


/**
 * register element details
 * add pad to element (2nd function called)
 */
static void gst_niftyled_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "niftyled",
    "Sink/Video/Device",
    "Output pixel data to LED-setups using libniftyled",
    "Daniel Hiepler <daniel-nlgst@niftylight.de>");

  gst_element_class_add_pad_template(element_class,
      gst_static_pad_template_get(&sink_factory));
}

/**
 * initialize the niftyled class (3rd function called)
 */
static void gst_niftyled_class_init (GstNiftyledClass * klass)
{
               
        /* property set/get handler */
        GObjectClass *gobject_class = (GObjectClass *) klass;
        gobject_class->set_property = GST_DEBUG_FUNCPTR(gst_niftyled_set_property);
        gobject_class->get_property = GST_DEBUG_FUNCPTR(gst_niftyled_get_property);
        //gobject_class->finalize = gst_niftyled_finalize;
        
        /* register sink-handlers */
        GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;
        gstbasesink_class->render = GST_DEBUG_FUNCPTR(gst_niftyled_render);
        gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_niftyled_start);
        gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_niftyled_stop);
        gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR(gst_niftyled_set_caps);

        
        /* register our properties */
        g_object_class_install_property(gobject_class, PROP_LOGLEVEL,
        g_param_spec_string("loglevel", "Log level", "set verbosity of niftyled output",
          "info", G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, PROP_CONFIG,
        g_param_spec_string("config", "config file", "niftyled config file with XML LED-setup description",
          "~/.gstniftyled.xml", G_PARAM_READWRITE));

        /** @todo register hw-plugin properties */
}

/**
 * initialize the new element (4th function called)
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void gst_niftyled_init(GstNiftyled * nl, GstNiftyledClass * gclass)
{                       
        /* default loglevel */
        nft_log_level_set(L_INFO);

		nl->prefs = led_prefs_init();
		
        /* default filename */
        led_prefs_default_filename(nl->configfile, sizeof(nl->configfile), ".gstniftyled.xml");

        nl->frame = NULL;
        strncpy(nl->format, "RGB u8", sizeof(nl->format));
        nl->setup = NULL;
        
        /* initialize babl */
        led_pixel_format_new();
}

/**
 * entry point to initialize the plug-in (1st function called)
 * initialize the plug-in itself
 * register the element factories and other features
 *
 * @todo register gstreamer logging function nft_log_set_function()
 */
static gboolean gst_niftyled_plugin_init(GstPlugin *plugin)
{        
	/* check binary version compatibility */
        if(!LED_CHECK_VERSION)
		return false;
	
        /* debug category for fltering log messages */
        GST_DEBUG_CATEGORY_INIT (gst_niftyled_debug, "niftyled", 0, 
                                "niftyled debugging category");
        
        return gst_element_register(plugin, "niftyled", GST_RANK_NONE, 
                              GST_TYPE_NIFTYLED);
}


/**
 * gstreamer looks for this structure to register niftyleds
 */
GST_PLUGIN_DEFINE 
(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "niftyled",
    "Output pixel data to LEDs using libniftyled",
    gst_niftyled_plugin_init,
    VERSION,
    "LGPL",
    "niftylight source build",
    PACKAGE_URL
)
