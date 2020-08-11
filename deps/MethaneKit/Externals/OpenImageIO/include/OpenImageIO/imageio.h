/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


/////////////////////////////////////////////////////////////////////////////
/// \file
///
/// Provides a simple API that abstracts the reading and writing of
/// images.  Subclasses, which may be found in DSO/DLL's, implement
/// particular formats.
///
/////////////////////////////////////////////////////////////////////////////

// clang-format off

#pragma once
#define OPENIMAGEIO_IMAGEIO_H

#if defined(_MSC_VER)
// Ignore warnings about DLL exported classes with member variables that are template classes.
// This happens with the std::vector<T> and std::string members of the classes below.
#    pragma warning(disable : 4251)
#endif

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <OpenImageIO/span.h>
#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/typedesc.h>

OIIO_NAMESPACE_BEGIN

class DeepData;
struct ROI;


/// Type we use for stride lengths.  This is only used to designate
/// pixel, scanline, tile, or image plane sizes in user-allocated memory,
/// so it doesn't need to represent sizes larger than can be malloced,
/// therefore ptrdiff_t seemed right.
typedef ptrdiff_t stride_t;


/// Type we use to express how many pixels (or bytes) constitute an image,
/// tile, or scanline.  Needs to be large enough to handle very big images
/// (which we presume could be > 4GB).
#if defined(LINUX64) || defined(_WIN64) || defined(__x86_64__) /* add others if we know for sure size_t is ok */
typedef size_t imagesize_t;
#else
typedef unsigned long long imagesize_t;
#endif



/// Special value to indicate a stride length that should be
/// auto-computed.
const stride_t AutoStride = std::numeric_limits<stride_t>::min();



/// Pointer to a function called periodically by read_image and
/// write_image.  This can be used to implement progress feedback, etc.
/// It takes an opaque data pointer (passed to read_image/write_image)
/// and a float giving the portion of work done so far.  It returns a
/// bool, which if 'true' will STOP the read or write.
typedef bool (*ProgressCallback)(void *opaque_data, float portion_done);



// Deprecated typedefs. Just use ParamValue and ParamValueList directly.
typedef ParamValue ImageIOParameter;
typedef ParamValueList ImageIOParameterList;



/// Helper struct describing a region of interest in an image.
/// The region is [xbegin,xend) x [begin,yend) x [zbegin,zend),
/// with the "end" designators signifying one past the last pixel,
/// a la STL style.
struct ROI {
    int xbegin, xend, ybegin, yend, zbegin, zend;
    int chbegin, chend;

    /// Default constructor is an undefined region. Note that this is also
    /// interpreted as All().
    constexpr ROI () : xbegin(std::numeric_limits<int>::min()), xend(0),
             ybegin(0), yend(0), zbegin(0), zend(0), chbegin(0), chend(0)
    { }

    /// Constructor with an explicitly defined region.
    ///
    constexpr ROI (int xbegin, int xend, int ybegin, int yend,
         int zbegin=0, int zend=1, int chbegin=0, int chend=10000)
        : xbegin(xbegin), xend(xend), ybegin(ybegin), yend(yend),
          zbegin(zbegin), zend(zend), chbegin(chbegin), chend(chend)
    { }

    /// Is a region defined?
    constexpr bool defined () const { return (xbegin != std::numeric_limits<int>::min()); }

    // Region dimensions.
    constexpr int width () const { return xend - xbegin; }
    constexpr int height () const { return yend - ybegin; }
    constexpr int depth () const { return zend - zbegin; }

    /// Number of channels in the region.  Beware -- this defaults to a
    /// huge number, and to be meaningful you must consider
    /// std::min (imagebuf.nchannels(), roi.nchannels()).
    constexpr int nchannels () const { return chend - chbegin; }

    /// Total number of pixels in the region.
    constexpr imagesize_t npixels () const {
        return defined()
            ? imagesize_t(width()) * imagesize_t(height()) * imagesize_t(depth())
            : 0;
    }

    /// All() is an alias for the default constructor, which indicates that
    /// it means "all" of the image, or no region restruction.  For example,
    ///     float myfunc (ImageBuf &buf, ROI roi = ROI::All());
    /// Note that this is equivalent to:
    ///     float myfunc (ImageBuf &buf, ROI roi = {});
    static constexpr ROI All () { return ROI(); }

    /// Test equality of two ROIs
    friend constexpr bool operator== (const ROI &a, const ROI &b) {
        return (a.xbegin == b.xbegin && a.xend == b.xend &&
                a.ybegin == b.ybegin && a.yend == b.yend &&
                a.zbegin == b.zbegin && a.zend == b.zend &&
                a.chbegin == b.chbegin && a.chend == b.chend);
    }
    /// Test inequality of two ROIs
    friend constexpr bool operator!= (const ROI &a, const ROI &b) {
        return (a.xbegin != b.xbegin || a.xend != b.xend ||
                a.ybegin != b.ybegin || a.yend != b.yend ||
                a.zbegin != b.zbegin || a.zend != b.zend ||
                a.chbegin != b.chbegin || a.chend != b.chend);
    }

    /// Test if the coordinate is within the ROI.
    constexpr bool contains (int x, int y, int z=0, int ch=0) const {
        return x >= xbegin && x < xend && y >= ybegin && y < yend
            && z >= zbegin && z < zend && ch >= chbegin && ch < chend;
    }

    /// Test if another ROI is entirely within our ROI.
    constexpr bool contains (const ROI& other) const {
        return (other.xbegin >= xbegin && other.xend <= xend &&
                other.ybegin >= ybegin && other.yend <= yend &&
                other.zbegin >= zbegin && other.zend <= zend &&
                other.chbegin >= chbegin && other.chend <= chend);
    }

    /// Stream output of the range
    friend std::ostream & operator<< (std::ostream &out, const ROI &roi) {
        out << roi.xbegin << ' ' << roi.xend << ' ' << roi.ybegin << ' '
            << roi.yend << ' ' << roi.zbegin << ' ' << roi.zend << ' '
            << roi.chbegin << ' ' << roi.chend;
        return out;
    }
};



/// Union of two regions, the smallest region containing both.
inline constexpr ROI roi_union (const ROI &A, const ROI &B) {
    return (A.defined() && B.defined())
        ? ROI (std::min (A.xbegin,  B.xbegin),  std::max (A.xend,  B.xend),
               std::min (A.ybegin,  B.ybegin),  std::max (A.yend,  B.yend),
               std::min (A.zbegin,  B.zbegin),  std::max (A.zend,  B.zend),
               std::min (A.chbegin, B.chbegin), std::max (A.chend, B.chend))
        : (A.defined() ? A : B);
}

/// Intersection of two regions.
inline constexpr ROI roi_intersection (const ROI &A, const ROI &B) {
    return (A.defined() && B.defined())
        ? ROI (std::max (A.xbegin,  B.xbegin),  std::min (A.xend,  B.xend),
               std::max (A.ybegin,  B.ybegin),  std::min (A.yend,  B.yend),
               std::max (A.zbegin,  B.zbegin),  std::min (A.zend,  B.zend),
               std::max (A.chbegin, B.chbegin), std::min (A.chend, B.chend))
        : (A.defined() ? A : B);
}




/// ImageSpec describes the data format of an image --
/// dimensions, layout, number and meanings of image channels.
class OIIO_API ImageSpec {
public:
    int x, y, z;              ///< origin (upper left corner) of pixel data
    int width;                ///< width of the pixel data window
    int height;               ///< height of the pixel data window
    int depth;                ///< depth of pixel data, >1 indicates a "volume"
    int full_x;               ///< origin of the full (display) window
    int full_y;               ///< origin of the full (display) window
    int full_z;               ///< origin of the full (display) window
    int full_width;           ///< width of the full (display) window
    int full_height;          ///< height of the full (display) window
    int full_depth;           ///< depth of the full (display) window
    int tile_width;           ///< tile width (0 for a non-tiled image)
    int tile_height;          ///< tile height (0 for a non-tiled image)
    int tile_depth;           ///< tile depth (0 for a non-tiled image,
                              ///<             1 for a non-volume image)
    int nchannels;            ///< number of image channels, e.g., 4 for RGBA
    TypeDesc format;          ///< data format of the channels
    std::vector<TypeDesc> channelformats;   ///< Optional per-channel formats
    std::vector<std::string> channelnames;  ///< Names for each channel,
                                            ///< e.g., {"R","G","B","A"}
    int alpha_channel;        ///< Index of alpha channel, or -1 if not known
    int z_channel;            ///< Index of depth channel, or -1 if not known
    bool deep;                ///< Contains deep data

    /// The above contains all the information that is likely needed for
    /// every image file, and common to all formats.  Rather than bloat
    /// this structure, customize it for new formats, or break back
    /// compatibility as we think of new things, we provide extra_attribs
    /// as a holder for any other properties of the image.  The public
    /// functions attribute and find_attribute may be used to access
    /// these data.  Note, however, that the names and semantics of such
    /// extra attributes are plugin-dependent and are not enforced by
    /// the imageio library itself.
    ParamValueList extra_attribs;  ///< Additional attributes

    /// Constructor: given just the data format, set all other fields to
    /// something reasonable.
    ImageSpec (TypeDesc format = TypeDesc::UNKNOWN);

    /// Constructor for simple 2D scanline image with nothing special.
    /// If fmt is not supplied, default to unsigned 8-bit data.
    ImageSpec (int xres, int yres, int nchans, TypeDesc fmt = TypeDesc::UINT8);

    /// Constructor from an ROI that gives x, y, z, and channel range, and
    /// a data format.
    explicit ImageSpec (const ROI &roi, TypeDesc fmt = TypeDesc::UINT8);

    /// Set the data format.
    void set_format (TypeDesc fmt);

    /// Set the channelnames to reasonable defaults ("R", "G", "B", "A"),
    /// and alpha_channel, based on the number of channels.
    void default_channel_names ();

    /// Return the number of bytes for each channel datum, assuming they
    /// are all stored using the data format given by this->format.
    size_t channel_bytes() const { return format.size(); }

    /// Return the number of bytes needed for the single specified
    /// channel.  If native is false (default), compute the size of one
    /// channel of this->format, but if native is true, compute the size
    /// of the channel in terms of the "native" data format of that
    /// channel as stored in the file.
    size_t channel_bytes (int chan, bool native=false) const;

    /// Return the number of bytes for each pixel (counting all channels).
    /// If native is false (default), assume all channels are in 
    /// this->format, but if native is true, compute the size of a pixel
    /// in the "native" data format of the file (these may differ in
    /// the case of per-channel formats).
    /// This will return std::numeric_limits<size_t>::max() in the
    /// event of an overflow where it's not representable in a size_t.
    size_t pixel_bytes (bool native=false) const;

    /// Return the number of bytes for just the subset of channels in
    /// each pixel described by [chbegin,chend).
    /// If native is false (default), assume all channels are in 
    /// this->format, but if native is true, compute the size of a pixel
    /// in the "native" data format of the file (these may differ in
    /// the case of per-channel formats).
    /// This will return std::numeric_limits<size_t>::max() in the
    /// event of an overflow where it's not representable in a size_t.
    size_t pixel_bytes (int chbegin, int chend, bool native=false) const;

    /// Return the number of bytes for each scanline.  This will return
    /// std::numeric_limits<imagesize_t>::max() in the event of an
    /// overflow where it's not representable in an imagesize_t.
    /// If native is false (default), assume all channels are in 
    /// this->format, but if native is true, compute the size of a pixel
    /// in the "native" data format of the file (these may differ in
    /// the case of per-channel formats).
    imagesize_t scanline_bytes (bool native=false) const;

    /// Return the number of pixels for a tile.  This will return
    /// std::numeric_limits<imagesize_t>::max() in the event of an
    /// overflow where it's not representable in an imagesize_t.
    imagesize_t tile_pixels () const;

    /// Return the number of bytes for each a tile of the image.  This
    /// will return std::numeric_limits<imagesize_t>::max() in the event
    /// of an overflow where it's not representable in an imagesize_t.
    /// If native is false (default), assume all channels are in 
    /// this->format, but if native is true, compute the size of a pixel
    /// in the "native" data format of the file (these may differ in
    /// the case of per-channel formats).
    imagesize_t tile_bytes (bool native=false) const;

    /// Return the number of pixels for an entire image.  This will
    /// return std::numeric_limits<imagesize_t>::max() in the event of
    /// an overflow where it's not representable in an imagesize_t.
    imagesize_t image_pixels () const;

    /// Return the number of bytes for an entire image.  This will
    /// return std::numeric_limits<image size_t>::max() in the event of
    /// an overflow where it's not representable in an imagesize_t.
    /// If native is false (default), assume all channels are in 
    /// this->format, but if native is true, compute the size of a pixel
    /// in the "native" data format of the file (these may differ in
    /// the case of per-channel formats).
    imagesize_t image_bytes (bool native=false) const;

    /// Verify that on this platform, a size_t is big enough to hold the
    /// number of bytes (and pixels) in a scanline, a tile, and the
    /// whole image.  If this returns false, the image is much too big
    /// to allocate and read all at once, so client apps beware and check
    /// these routines for overflows!
    bool size_t_safe() const {
        const imagesize_t big = std::numeric_limits<size_t>::max();
        return image_bytes() < big && scanline_bytes() < big &&
            tile_bytes() < big;
    }

    /// Adjust the stride values, if set to AutoStride, to be the right
    /// sizes for contiguous data with the given format, channels,
    /// width, height.
    static void auto_stride (stride_t &xstride, stride_t &ystride,
                             stride_t &zstride, stride_t channelsize,
                             int nchannels, int width, int height) {
        if (xstride == AutoStride)
            xstride = nchannels * channelsize;
        if (ystride == AutoStride)
            ystride = xstride * width;
        if (zstride == AutoStride)
            zstride = ystride * height;
    }

    /// Adjust the stride values, if set to AutoStride, to be the right
    /// sizes for contiguous data with the given format, channels,
    /// width, height.
    static void auto_stride (stride_t &xstride, stride_t &ystride,
                             stride_t &zstride, TypeDesc format,
                             int nchannels, int width, int height) {
        auto_stride (xstride, ystride, zstride, format.size(),
                     nchannels, width, height);
    }

    /// Adjust xstride, if set to AutoStride, to be the right size for
    /// contiguous data with the given format and channels.
    static void auto_stride (stride_t &xstride, TypeDesc format, int nchannels) {
        if (xstride == AutoStride)
            xstride = nchannels * format.size();
    }

    /// Add an optional attribute to the extra attribute list
    ///
    void attribute (string_view name, TypeDesc type, const void *value);

    /// Add an optional attribute to the extra attribute list.
    ///
    void attribute (string_view name, TypeDesc type, string_view value);

    /// Add an unsigned int attribute
    ///
    void attribute (string_view name, unsigned int value) {
        attribute (name, TypeDesc::UINT, &value);
    }

    /// Add an int attribute
    ///
    void attribute (string_view name, int value) {
        attribute (name, TypeDesc::INT, &value);
    }

    /// Add a float attribute
    ///
    void attribute (string_view name, float value) {
        attribute (name, TypeDesc::FLOAT, &value);
    }

    /// Add a string attribute
    ///
    void attribute (string_view name, string_view value) {
        const char *s = value.c_str();
        attribute (name, TypeDesc::STRING, &s);
    }

    /// Remove the specified attribute from the list of extra_attribs. If
    /// not found, do nothing.  If searchtype is anything but UNKNOWN,
    /// restrict matches to only those of the given type. If casesensitive
    /// is true, the name search will be case-sensitive, otherwise the name
    /// search will be performed without regard to case (this is the
    /// default).
    void erase_attribute (string_view name,
                          TypeDesc searchtype=TypeDesc::UNKNOWN,
                          bool casesensitive=false);

    /// Search for an attribute of the given name in the list of
    /// extra_attribs. If searchtype is anything but UNKNOWN, restrict
    /// matches to only those of the given type. If casesensitive is true,
    /// the name search will be case-sensitive, otherwise the name search
    /// will be performed without regard to case (this is the default).
    ParamValue * find_attribute (string_view name,
                                 TypeDesc searchtype=TypeDesc::UNKNOWN,
                                 bool casesensitive=false);
    const ParamValue *find_attribute (string_view name,
                                      TypeDesc searchtype=TypeDesc::UNKNOWN,
                                      bool casesensitive=false) const;

    /// Search for the named attribute and return a pointer to an
    /// ParamValue record, or NULL if not found.  This variety of
    /// find_attribute() can retrieve items such as "width", which are part
    /// of the ImageSpec, but not in extra_attribs. The tmpparam is a
    /// temporary storage area owned by the caller, which is used as
    /// temporary buffer in cases where the information does not correspond
    /// to an actual extra_attribs (in this case, the return value will be
    /// &tmpparam).
    const ParamValue * find_attribute (string_view name,
                         ParamValue &tmpparam,
                         TypeDesc searchtype=TypeDesc::UNKNOWN,
                         bool casesensitive=false) const;

    /// Simple way to get an integer attribute, with default provided.
    /// Automatically will return an int even if the data is really
    /// unsigned, short, or byte.
    int get_int_attribute (string_view name, int defaultval=0) const;

    /// Simple way to get a float attribute, with default provided.
    /// Automatically will return a float even if the data is really
    /// double or half.
    float get_float_attribute (string_view name, float defaultval=0) const;

    /// Simple way to get a string attribute, with default provided.
    ///
    string_view get_string_attribute (string_view name,
                           string_view defaultval = string_view()) const;

    /// For a given parameter p, format the value nicely as a string.  If
    /// 'human' is true, use especially human-readable explanations (units,
    /// or decoding of values) for certain known metadata.
    static std::string metadata_val (const ParamValue &p, bool human=false);

    enum SerialFormat  { SerialText, SerialXML };
    enum SerialVerbose { SerialBrief, SerialDetailed, SerialDetailedHuman };

    /// Convert ImageSpec class into a serialized string.
    std::string serialize (SerialFormat format,
                           SerialVerbose verbose = SerialDetailed) const;

    /// Convert ImageSpec class into XML string.
    ///
    std::string to_xml () const;

    /// Get an ImageSpec class from XML string.
    ///
    void from_xml (const char *xml);

    /// Hunt for the "Compression" and "CompressionQuality" settings in the
    /// spec and turn them into the compression name and quality. This
    /// handles compression name/qual combos of the form "name:quality".
    std::pair<string_view, int>
    decode_compression_metadata(string_view defaultcomp = "",
                                int defaultqual = -1) const;

    /// Helper function to verify that the given pixel range exactly covers
    /// a set of tiles.  Also returns false if the spec indicates that the
    /// image isn't tiled at all.
    bool valid_tile_range (int xbegin, int xend, int ybegin, int yend,
                           int zbegin, int zend) {
        return (tile_width &&
                ((xbegin-x) % tile_width)  == 0 &&
                ((ybegin-y) % tile_height) == 0 &&
                ((zbegin-z) % tile_depth)  == 0 &&
                (((xend-x) % tile_width)  == 0 || (xend-x) == width) &&
                (((yend-y) % tile_height) == 0 || (yend-y) == height) &&
                (((zend-z) % tile_depth)  == 0 || (zend-z) == depth));
    }

    /// Return the channelformat of the given channel. This is safe even
    /// if channelformats is not filled out.
    TypeDesc channelformat (int chan) const {
        return chan >= 0 && chan < (int)channelformats.size()
            ? channelformats[chan] : format;
    }

    /// Return the channel name of the given channel. This is safe even if
    /// channelnames is not filled out.
    string_view channel_name (int chan) const {
        return chan >= 0 && chan < (int)channelnames.size()
            ? string_view(channelnames[chan]) : "";
    }

    /// Fill in an array of channel formats describing all channels in
    /// the image.  (Note that this differs slightly from the member
    /// data channelformats, which is empty if there are not separate
    /// per-channel formats.)
    void get_channelformats (std::vector<TypeDesc> &formats) const {
        formats = channelformats;
        if ((int)formats.size() < nchannels)
            formats.resize (nchannels, format);
    }

    /// Return the index of the named channel, or -1 if not found.
    int channelindex (string_view name) const;

    /// Return pixel data window for this ImageSpec as a ROI.
    ROI roi () const {
        return ROI (x, x+width, y, y+height, z, z+depth, 0, nchannels);
    }

    /// Return full/display window for this ImageSpec as a ROI.
    ROI roi_full () const {
        return ROI (full_x, full_x+full_width, full_y, full_y+full_height,
                    full_z, full_z+full_depth, 0, nchannels);
    }

    /// Set pixel data window parameters (x, y, z, width, height, depth)
    /// for this ImageSpec from an ROI.
    /// Does NOT change the channels of the spec, regardless of r.
    void set_roi (const ROI &r) {
        x = r.xbegin;
        y = r.ybegin;
        z = r.zbegin;
        width = r.width();
        height = r.height();
        depth = r.depth();
    }

    /// Set full/display window parameters (full_x, full_y, full_z,
    /// full_width, full_height, full_depth) for this ImageSpec from an ROI.
    /// Does NOT change the channels of the spec, regardless of r.
    void set_roi_full (const ROI &r) {
        full_x = r.xbegin;
        full_y = r.ybegin;
        full_z = r.zbegin;
        full_width = r.width();
        full_height = r.height();
        full_depth = r.depth();
    }

    /// Copy the dimensions (x, y, z, width, height, depth, full*,
    /// nchannels, format) and types of the other ImageSpec. Be careful,
    /// this doesn't copy channelnames or the metadata in extra_attribs.
    void copy_dimensions (const ImageSpec &other) {
        x = other.x;
        y = other.y;
        z = other.z;
        width = other.width;
        height = other.height;
        depth = other.depth;
        full_x = other.full_x;
        full_y = other.full_y;
        full_z = other.full_z;
        full_width = other.full_width;
        full_height = other.full_height;
        full_depth = other.full_depth;
        tile_width = other.tile_width;
        tile_height = other.tile_height;
        tile_depth = other.tile_depth;
        nchannels = other.nchannels;
        format = other.format;
        channelformats = other.channelformats;
        alpha_channel = other.alpha_channel;
        z_channel = other.z_channel;
        deep = other.deep;
    }

    /// Is this ImageSpec undefined? (Designated by no channels and
    /// undefined data type -- true of the uninitialized state of an
    /// ImageSpec, and presumably not for any ImageSpec that is useful or
    /// purposefully made.)
    bool undefined () const {
        return nchannels == 0 && format == TypeUnknown;
    }
};




/// ImageInput abstracts the reading of an image file in a file
/// format-agnostic manner.
class OIIO_API ImageInput {
public:
    /// unique_ptr to an ImageInput
    using unique_ptr = std::unique_ptr<ImageInput>;

    /// Create an ImageInput subclass instance that is able to read the
    /// given file and open it, returning a unique_ptr to the ImageInput if
    /// successful.  The unique_ptr is set up with an appropriate deleter so
    /// the ImageInput will be properly deleted when the unique_ptr goes out
    /// of scope or is reset. If the open fails, return an empty unique_ptr
    /// and set an error that can be retrieved by OpenImageIO::geterror().
    ///
    /// The 'config', if not nullptr, points to an ImageSpec giving
    /// requests or special instructions.  ImageInput implementations
    /// are free to not respond to any such requests, so the default
    /// implementation is just to ignore config.
    ///
    /// open() will first try to make an ImageInput corresponding to
    /// the format implied by the file extension (for example, "foo.tif"
    /// will try the TIFF plugin), but if one is not found or if the
    /// inferred one does not open the file, every known ImageInput type
    /// will be tried until one is found that will open the file.
    static unique_ptr open (const std::string &filename,
                            const ImageSpec *config = nullptr);

    /// Create and return an ImageInput implementation that is able
    /// to read the given file.  If do_open is true, fully open it if
    /// possible (using the optional configuration spec, if supplied),
    /// otherwise just create the ImageInput but don't open it.
    /// The plugin_searchpath parameter is an override of the searchpath.
    /// colon-separated list of directories to search for ImageIO plugin
    /// DSO/DLL's (not a searchpath for the image itself!).  This will
    /// actually just try every imageio plugin it can locate, until it
    /// finds one that's able to open the file without error.
    ///
    /// If the caller intends to immediately open the file, then it is
    /// often simpler to call static ImageInput::open().
    static unique_ptr create (const std::string &filename, bool do_open=false,
                              const ImageSpec *config=nullptr,
                              string_view plugin_searchpath="");
    static unique_ptr create (const std::string &filename,
                              const std::string &plugin_searchpath);

    // Destructor for a raw ImageInput.
    static void destroy (ImageInput *x);

    ImageInput ();
    virtual ~ImageInput ();

    typedef std::recursive_mutex mutex;
    typedef std::lock_guard<mutex> lock_guard;

    /// Return the name of the format implemented by this class.
    ///
    virtual const char *format_name (void) const = 0;

    /// Given the name of a 'feature', return whether this ImageInput
    /// supports input of images with the given properties. Most queries
    /// will simply return 0 for "doesn't support" and nonzero for "supports
    /// it", but it is acceptable to have queries return other nonzero
    /// integers to indicate varying degrees of support or limits (but
    /// should be clearly documented as such).
    ///
    /// Feature names that ImageIO plugins are expected to recognize
    /// include:
    ///    "arbitrary_metadata" Does this format allow metadata with
    ///                        arbitrary names and types?
    ///    "exif"           Can this format store Exif camera data?
    ///    "iptc"           Can this format store IPTC data?
    ///    "procedural"     Can this format create images without reading
    ///                        from a disk file?
    ///    "ioproxy"        Does this format reader support IOProxy?
    ///
    /// Note that main advantage of this approach, versus having
    /// separate individual supports_foo() methods, is that this allows
    /// future expansion of the set of possible queries without changing
    /// the API, adding new entry points, or breaking linkage
    /// compatibility.
    virtual int supports (string_view feature) const { return false; }

    /// Return true if the named file is file of the type for this
    /// ImageInput.  The implementation will try to determine this as
    /// efficiently as possible, in most cases much less expensively
    /// than doing a full open().  Note that a file can appear to be of
    /// the right type (i.e., valid_file() returning true) but still
    /// fail a subsequent call to open(), such as if the contents of the
    /// file are truncated, nonsensical, or otherwise corrupted.
    virtual bool valid_file (const std::string &filename) const;

    /// Open file with given name.  Various file attributes are put in
    /// newspec and a copy is also saved in this->spec.  From these
    /// attributes, you can discern the resolution, if it's tiled,
    /// number of channels, and native data format.  Return true if the
    /// file was found and opened okay.
    virtual bool open (const std::string &name, ImageSpec &newspec) = 0;

    /// Open file with given name, similar to open(name,newspec). The
    /// 'config' is an ImageSpec giving requests or special
    /// instructions.  ImageInput implementations are free to not
    /// respond to any such requests, so the default implementation is
    /// just to ignore config and call regular open(name,newspec).
    virtual bool open (const std::string &name, ImageSpec &newspec,
                       const ImageSpec & /*config*/) { return open(name,newspec); }

    /// Return a reference to the image format specification of the
    /// current subimage/MIPlevel.  Note that the contents of the spec
    /// are invalid before open() or after close(), and may change with
    /// a call to seek_subimage(). It is thus not thread-safe, since the
    /// spec may change if another thread calls seek_subimage, or any of
    /// the read_*() functions that take explicit subimage/miplevel.
    virtual const ImageSpec &spec (void) const { return m_spec; }

    /// Return a full copy of the ImageSpec of the designated subimage and
    /// MIPlevel. This method is thread-safe, but it is potentially
    /// expensive, due to the work that needs to be done to fully copy an
    /// ImageSpec if there is lots of named metadata to allocate and copy.
    /// See also the less expensive spec_dimensions(). Errors (such as
    /// having requested a nonexistant subimage) are indicated by returning
    /// an ImageSpec with format==TypeUnknown.
    virtual ImageSpec spec (int subimage, int miplevel=0);

    /// Return a copy of the ImageSpec of the designated subimage and
    /// miplevel, but only the dimension and type fields. Just as with a
    /// call to ImageSpec::copy_dimensions(), neither the channel names nor
    /// any of the arbitrary named metadata will be copied, thus this is a
    /// relatively inexpensive operation if you don't need that information.
    /// It is guaranteed to be thread-safe. Errors (such as having requested
    /// a nonexistant subimage) are indicated by returning an ImageSpec with
    /// format==TypeUnknown.
    virtual ImageSpec spec_dimensions (int subimage, int miplevel=0);

    /// Close an image that we are totally done with. The call to close() is
    /// not strictly necessary if the ImageInput is destroyed immediately
    /// afterwards, since it is required for the destructor to close if the
    /// file is still open.
    virtual bool close () = 0;

    /// Returns the index of the subimage that is currently being read.
    /// The first subimage (or the only subimage, if there is just one)
    /// is number 0.
    virtual int current_subimage (void) const { return 0; }

    /// Returns the index of the MIPmap image that is currently being read.
    /// The highest-res MIP level (or the only level, if there is just
    /// one) is number 0.
    virtual int current_miplevel (void) const { return 0; }

    /// Seek to the given subimage and MIP-map level within the open image
    /// file.  The first subimage of the file has index 0, the highest-
    /// resolution MIP level has index 0.  Return true on success, false on
    /// failure (including that there is not a subimage or MIP level with
    /// the specified index).  The new subimage's vital statistics are put
    /// in *newspec (if not NULL) and may also be retrieved by  in
    /// this->spec()).  The reader is expected to give the appearance of
    /// random access to subimages and MIP levels -- in other words, if it
    /// can't randomly seek to the given subimage/level, it should
    /// transparently close, reopen, and sequentially read through prior
    /// subimages and levels.
    virtual bool seek_subimage (int subimage, int miplevel) {
        // Default implementation assumes no support for subimages or
        // mipmaps, so there is no work to do.
        return subimage == current_subimage() && miplevel == current_miplevel();
    }

    // Old version for backwards-compatibility: pass reference to newspec.
    // Some day this will be deprecated.
    bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec) {
        bool ok = seek_subimage (subimage, miplevel);
        if (ok)
            newspec = spec();
        return ok;
    }

    // Seek to the given subimage -- backwards-compatible call that
    // doesn't worry about MIP-map levels at all.
    // Some day this will be deprecated.
    bool seek_subimage (int subimage, ImageSpec &newspec) {
        return seek_subimage (subimage, 0 /* miplevel */, newspec);
    }

    /// Read the scanline that includes pixels (*,y,z) into data,
    /// converting if necessary from the native data format of the file
    /// into the 'format' specified (z==0 for non-volume images).  The
    /// stride value gives the data spacing of adjacent pixels (in
    /// bytes).  Strides set to AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    /// If format is TypeDesc::UNKNOWN, then rather than converting to
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// The reader is expected to give the appearance of random access --
    /// in other words, if it can't randomly seek to the given scanline,
    /// it should transparently close, reopen, and sequentially read
    /// through prior scanlines.  The base ImageInput class has a
    /// default implementation that calls read_native_scanline and then
    /// does appropriate format conversion, so there's no reason for
    /// each format plugin to override this method.
    virtual bool read_scanline (int y, int z, TypeDesc format, void *data,
                                stride_t xstride=AutoStride);

    ///
    /// Simple read_scanline reads to contiguous float pixels.
    bool read_scanline (int y, int z, float *data) {
        return read_scanline (y, z, TypeDesc::FLOAT, data);
    }

    /// Read multiple scanlines that include pixels (*,y,z) for all ybegin
    /// <= y < yend in the specified subimage and mip level, into data,
    /// using the strides given and converting to the requested data format
    /// (unless format is TypeDesc::UNKNOWN, in which case pixels will be
    /// copied in the native data layout, including per-channel data
    /// formats).  Only channels [chbegin,chend) will be read/copied
    /// (chbegin=0, chend=spec.nchannels reads all channels, yielding
    /// equivalent behavior to the simpler variant of read_scanlines).
    ///
    /// This version of read_scanlines, because it passes explicit
    /// subimage/miplevel, does not require a separate call to
    /// seek_subimage, and is guaranteed to be thread-safe against other
    /// concurrent calls to any of the read_* methods that take an explicit
    /// subimage/miplevel (but not against any other ImageInput methods).
    virtual bool read_scanlines (int subimage, int miplevel,
                                 int ybegin, int yend, int z,
                                 int chbegin, int chend,
                                 TypeDesc format, void *data,
                                 stride_t xstride=AutoStride,
                                 stride_t ystride=AutoStride);

    // DEPRECATED versions of read_scanlines (pre-1.9 OIIO). These will
    // eventually be removed. Try to replace these calls with ones to the
    // new variety of read_scanlines that takes an explicit subimage and
    // miplevel. These old versions are NOT THREAD-SAFE.
    bool read_scanlines (int ybegin, int yend, int z,
                         TypeDesc format, void *data,
                         stride_t xstride=AutoStride,
                         stride_t ystride=AutoStride);
    bool read_scanlines (int ybegin, int yend, int z,
                         int chbegin, int chend,
                         TypeDesc format, void *data,
                         stride_t xstride=AutoStride,
                         stride_t ystride=AutoStride);

    /// Read the tile whose upper-left origin is (x,y,z) into data,
    /// converting if necessary from the native data format of the file
    /// into the 'format' specified. (z==0 for non-volume images.) The
    /// stride values give the data spacing of adjacent pixels, scanlines,
    /// and volumetric slices (measured in bytes). Strides set to AutoStride
    /// imply 'contiguous' data in the shape of a full tile, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.tile_width
    ///     zstride == ystride*spec.tile_height
    /// If format is TypeDesc::UNKNOWN, then rather than converting to
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// The reader is expected to give the appearance of random access
    /// -- in other words, if it can't randomly seek to the given tile,
    /// it should transparently close, reopen, and sequentially read
    /// through prior tiles.  The base ImageInput class has a default
    /// implementation that calls read_native_tile and then does
    /// appropriate format conversion, so there's no reason for each
    /// format plugin to override this method.
    virtual bool read_tile (int x, int y, int z, TypeDesc format,
                            void *data, stride_t xstride=AutoStride,
                            stride_t ystride=AutoStride,
                            stride_t zstride=AutoStride);

    ///
    /// Simple read_tile reads to contiguous float pixels.
    bool read_tile (int x, int y, int z, float *data) {
        return read_tile (x, y, z, TypeDesc::FLOAT, data,
                          AutoStride, AutoStride, AutoStride);
    }

    /// Read the block of tiles that include all pixels and channels in the
    /// ROI, at the specified subimage and MIP level. The values are
    /// converted to the requested data format (unless format is
    /// TypeDesc::UNKNOWN, in which case pixels will be copied in the native
    /// data layout, including per-channel data formats), and stored in
    /// `data` according to the optional stride lengths (in bytes).
    ///
    /// The stride values give the data spacing of adjacent pixels,
    /// scanlines, and volumetric slices (measured in bytes). Strides set to
    /// AutoStride imply 'contiguous' data in the shape of the [begin,end)
    /// region, i.e.,
    ///     xstride == (chend - chbegin) * format.size()
    ///     ystride == xstride * (xend - xbegin)
    ///     zstride == ystride * (yend - ybegin)
    ///
    /// This version of read_tiles, because it passes explicit
    /// subimage/miplevel, does not require a separate call to
    /// seek_subimage, and is guaranteed to be thread-safe against other
    /// concurrent calls to any of the read_* methods that take an explicit
    /// subimage/miplevel (but not against any other ImageInput methods).
    virtual bool read_tiles (int subimage, int miplevel, int xbegin, int xend,
                             int ybegin, int yend, int zbegin, int zend,
                             int chbegin, int chend, TypeDesc format, void *data,
                             stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride);

    // DEPRECATED versions of read_tiles (pre-1.9 OIIO). These will
    // eventually be removed. Try to replace these calls with ones to the
    // new variety of read_tiles that takes an explicit subimage and
    // miplevel. These old versions are NOT THREAD-SAFE.
    bool read_tiles (int xbegin, int xend, int ybegin, int yend,
                     int zbegin, int zend, TypeDesc format, void *data,
                     stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                     stride_t zstride=AutoStride);
    bool read_tiles (int xbegin, int xend, int ybegin, int yend,
                     int zbegin, int zend, int chbegin, int chend,
                     TypeDesc format, void *data, stride_t xstride=AutoStride,
                     stride_t ystride=AutoStride, stride_t zstride=AutoStride);

    /// Read the entire image of spec.width x spec.height x spec.depth
    /// pixels into data (which must already be sized large enough for
    /// the entire image) with the given strides and in the desired
    /// format.  Read tiles or scanlines automatically.  Strides set to
    /// AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.width
    ///     zstride == ystride*spec.height
    /// If format is TypeDesc::UNKNOWN, then rather than converting to
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// Because this may be an expensive operation, a progress callback
    /// may be passed.  Periodically, it will be called as follows:
    ///     progress_callback (progress_callback_data, float done)
    /// where 'done' gives the portion of the image
    virtual bool read_image (TypeDesc format, void *data,
                             stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride,
                             ProgressCallback progress_callback=NULL,
                             void *progress_callback_data=NULL);

    /// Read the entire image of spec.width x spec.height x spec.depth
    /// pixels into data (which must already be sized large enough for
    /// the entire image) with the given strides and in the desired
    /// format.  Read tiles or scanlines automatically. Only channels
    /// [chbegin,chend) will be read/copied (chbegin=0, chend=spec.nchannels
    /// reads all channels, yielding equivalent behavior to the simpler
    /// variant of read_image).
    virtual bool read_image (int subimage, int miplevel,
                             int chbegin, int chend,
                             TypeDesc format, void *data,
                             stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride,
                             ProgressCallback progress_callback=NULL,
                             void *progress_callback_data=NULL);
    // DEPRECATED versions of read_image (pre-1.9 OIIO). These will
    // eventually be removed. Try to replace these calls with ones to the
    // new variety of read_tiles that takes an explicit subimage and
    // miplevel. These old versions are NOT THREAD-SAFE.
    virtual bool read_image (int chbegin, int chend,
                             TypeDesc format, void *data,
                             stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride,
                             ProgressCallback progress_callback=NULL,
                             void *progress_callback_data=NULL);

    ///
    /// Simple read_image reads to contiguous float pixels.
    bool read_image (float *data) {
        return read_image (TypeDesc::FLOAT, data);
    }


    ////////////////////////////////////////////////////////////////////////
    // read_native_* methods are usually not directly called by user code
    // (except for read_native_deep_* varieties). These are the methods that
    // are overloaded by the ImageInput subclasses that implement the
    // individual file format readers.
    //
    // The read_native_* methods always read the "native" data types
    // (including per-channel data types) and assume that `data` points to
    // contiguous memory (no non-default strides). In contrast, the
    // read_scanline/scanlines/tile/tiles handle data type translation and
    // arbitrary strides.
    //
    // The read_native_* methods take an explicit subimage and miplevel, and
    // thus do not require a prior call to seek_subimage (and therefore no
    // saved state). They are all required to be thread-safe when called
    // concurrently with any other read_native_* call or with the varieties
    // of read_tiles() that also takes an explicit subimage and miplevel
    // parameter.
    //
    // As far as format-reading ImageInput subclasses are concerned, the
    // only truly required overloads are read_native_scanline (always) and
    // read_native_tile (only for formats that support tiles). The other
    // varieties are special cases, for example if the particular format is
    // able to efficiently read multiple scanlines or tiles at once, and if
    // the subclass does not provide overloads, the base class
    // implementaiton will be used instead, which is implemented by reducing
    // the operation to multiple calls to read_scanline or read_tile.

    /// Read a single scanline of native data into contiguous memory.
    virtual bool read_native_scanline (int subimage, int miplevel,
                                       int y, int z, void *data) = 0;

    virtual bool read_native_scanlines (int subimage, int miplevel,
                                        int ybegin, int yend, int z,
                                        void *data);
    virtual bool read_native_scanlines (int subimage, int miplevel,
                                        int ybegin, int yend, int z,
                                        int chbegin, int chend, void *data);

    /// Read a single tile (all channels) of native data into contiguous
    /// mamory. The base class read_native_tile fails. A format reader that
    /// supports tiles MUST overload this virtual method that reads a single
    /// tile (all channels).
    virtual bool read_native_tile (int subimage, int miplevel,
                                   int x, int y, int z, void *data);

    /// Read multiple tiles (all channels) of native data into contigious
    /// memory. A format reader that supports reading multiple tiles at once
    /// (in a way that's more efficient than reading the tiles one at a
    /// time) is advised (but not required) to overload this virtual method.
    /// If an ImageInput subclass does not overload this, the default
    /// implementation here is simply to loop over the tiles, calling the
    /// single-tile read_native_tile() for each one.
    virtual bool read_native_tiles (int subimage, int miplevel,
                                    int xbegin, int xend, int ybegin, int yend,
                                    int zbegin, int zend, void *data);

    /// Read multiple tiles (potentially a subset of channels) of native
    /// data into contigious memory. A format reader that supports reading
    /// multiple tiles at once, and can handle a channel subset while doing
    /// so, is advised (but not required) to overload this virtual method.
    /// If an ImageInput subclass does not overload this, the default
    /// implementation here is simply to loop over the tiles, calling the
    /// single-tile read_native_tile() for each one (and copying carefully
    /// to handle the channel subset issues).
    virtual bool read_native_tiles (int subimage, int miplevel,
                                    int xbegin, int xend, int ybegin, int yend,
                                    int zbegin, int zend,
                                    int chbegin, int chend, void *data);

    /// Read into deepdata the block of native deep scanlines corresponding
    /// to pixels (*,y,z) for all roi.ybegin <= y < roi.yend, into deepdata.
    /// Only channels [roi.chbegin, chend) will be read (roi.chbegin=0,
    /// roi.chend=spec.nchannels reads all channels). The x range must be
    /// the full width of the image data, and the z range must be just one
    /// depth plane.
    virtual bool read_native_deep_scanlines (int subimage, int miplevel,
                                             int ybegin, int yend, int z,
                                             int chbegin, int chend,
                                             DeepData &deepdata);

    /// Read into deepdata the block of native deep data tiles that include
    /// all pixels and channels specified by the ROI.
    virtual bool read_native_deep_tiles (int subimage, int miplevel,
                                         int xbegin, int xend,
                                         int ybegin, int yend,
                                         int zbegin, int zend,
                                         int chbegin, int chend,
                                         DeepData &deepdata);

    /// Read the entire deep data image of spec.width x spec.height x
    /// spec.depth pixels, all channels, into deepdata.
    virtual bool read_native_deep_image (int subimage, int miplevel,
                                         DeepData &deepdata);

    // DEPRECATED(1.9), Now just used for back compatibility:
    bool read_native_deep_scanlines (int ybegin, int yend, int z,
                             int chbegin, int chend, DeepData &deepdata) {
        return read_native_deep_scanlines (current_subimage(), current_miplevel(),
                                           ybegin, yend, z,
                                           chbegin, chend, deepdata);
    }
    bool read_native_deep_tiles (int xbegin, int xend, int ybegin, int yend,
                                 int zbegin, int zend, int chbegin, int chend,
                                 DeepData &deepdata) {
        return read_native_deep_tiles (current_subimage(), current_miplevel(),
                                       xbegin, xend, ybegin, yend,
                                       zbegin, zend, chbegin, chend, deepdata);
    }
    bool read_native_deep_image (DeepData &deepdata) {
        return read_native_deep_image (current_subimage(), current_miplevel(),
                                       deepdata);
    }


    /// General message passing between client and image input server
    ///
    virtual int send_to_input (const char *format, ...);
    int send_to_client (const char *format, ...);

    /// If any of the API routines returned false indicating an error,
    /// this routine will return the error string (and clear any error
    /// flags).  If no error has occurred since the last time geterror()
    /// was called, it will return an empty string.
    std::string geterror () const {
        lock_guard lock (m_mutex);
        std::string e = m_errmessage;
        m_errmessage.clear ();
        return e;
    }

    /// Error reporting for the plugin implementation: call this with
    /// Strutil::format-like arguments.
    /// Use with caution! Some day this will change to be fmt-like rather
    /// than printf-like.
    template<typename... Args>
    void error(const char* fmt, const Args&... args) const {
        append_error(Strutil::format (fmt, args...));
    }

    /// Error reporting for the plugin implementation: call this with
    /// printf-like arguments.
    template<typename... Args>
    void errorf(const char* fmt, const Args&... args) const {
        append_error(Strutil::sprintf (fmt, args...));
    }

    /// Error reporting for the plugin implementation: call this with
    /// fmt::format-like arguments.
    template<typename... Args>
    void fmterror(const char* fmt, const Args&... args) const {
        append_error(Strutil::fmt::format (fmt, args...));
    }

    /// Set the current thread-spawning policy: the maximum number of
    /// threads that may be spawned by ImageInput internals. A value of 1
    /// means all work will be done by the calling thread; 0 means to use
    /// the global OIIO::attribute("threads") value.
    void threads (int n) { m_threads = n; }

    /// Retrieve the current thread-spawning policy.
    int threads () const { return m_threads; }

    /// Lock, try_lock, and unlock for the internal mutex.
    void lock () { m_mutex.lock(); }
    bool try_lock () { return m_mutex.try_lock(); }
    void unlock () { m_mutex.unlock(); }

    // Custom new and delete to ensure that allocations & frees happen in
    // the OpenImageIO library, not in the app or plugins (because Windows).
    void* operator new (size_t size);
    void operator delete (void *ptr);

    /// Call signature of a function that creates and returns an ImageInput*
    typedef ImageInput* (*Creator)();

protected:
    mutable mutex m_mutex;   // lock of the thread-safe methods
    ImageSpec m_spec;  // format spec of the current open subimage/MIPlevel
                       // BEWARE using m_spec directly -- not thread-safe

private:
    mutable std::string m_errmessage;  // private storage of error message
    int m_threads;    // Thread policy
    void append_error (const std::string& message) const; // add to m_errmessage
    // Deprecated:
    static unique_ptr create (const std::string &filename, bool do_open,
                              const std::string &plugin_searchpath);
};




/// ImageOutput abstracts the writing of an image file in a file
/// format-agnostic manner.
class OIIO_API ImageOutput {
public:
    /// unique_ptr to an ImageOutput, with custom deleter.
    using unique_ptr = std::unique_ptr<ImageOutput>;

    /// Create an ImageOutput that will write to a file, with the format
    /// inferred from the extension of the name.  The plugin_searchpath
    /// parameter is a colon-separated list of directories to search for
    /// ImageIO plugin DSO/DLL's.  This just creates the ImageOutput, it
    /// does not open the file.  If the writer could be created, a
    /// unique_ptr to it will be returned, otherwise an empty pointer will
    /// be returned and an appropriate error message will be set that can be
    /// retrieved with OIIO::geterror().
    static unique_ptr create (const std::string &filename,
                              const std::string &plugin_searchpath="");

    // Destructor for a raw ImageOutput.
    static void destroy (ImageOutput *x);

    ImageOutput ();
    virtual ~ImageOutput ();

    /// Return the name of the format implemented by this class.
    ///
    virtual const char *format_name (void) const = 0;

    // Overrride these functions in your derived output class
    // to inform the client which formats are supported

    /// Given the name of a 'feature', return whether this ImageOutput
    /// supports output of images with the given properties. Most queries
    /// will simply return 0 for "doesn't support" and nonzero for "supports
    /// it", but it is acceptable to have queries return other nonzero
    /// integers to indicate varying degrees of support or limits (but
    /// should be clearly documented as such).
    ///
    /// Feature names that ImageIO plugins are expected to recognize
    /// include:
    ///    "tiles"          Is this format able to write tiled images?
    ///    "rectangles"     Does this plugin accept arbitrary rectangular
    ///                       pixel regions, not necessarily aligned to
    ///                       scanlines or tiles?
    ///    "random_access"  May tiles or scanlines be written in
    ///                       any order (false indicates that they MUST
    ///                       be in successive order).
    ///    "multiimage"     Does this format support multiple subimages
    ///                       within a file?
    ///    "appendsubimage" Does this format support adding subimages one at
    ///                       a time through open(name,spec,AppendSubimage)?
    ///                       If not, then open(name,subimages,specs) must
    ///                       be used instead.
    ///    "mipmap"         Does this format support multiple resolutions
    ///                       for an image/subimage?
    ///    "volumes"        Does this format support "3D" pixel arrays?
    ///    "alpha"          Can this format support an alpha channel?
    ///    "nchannels"      Can this format support arbitrary number of
    ///                        channels (beyond RGBA)?
    ///    "rewrite"        May the same scanline or tile be sent more than
    ///                       once?  (Generally, this will be true for
    ///                       plugins that implement interactive display.)
    ///    "empty"          Does this plugin support passing a NULL data
    ///                       pointer to write_scanline or write_tile to
    ///                       indicate that the entire data block is zero?
    ///    "channelformats" Does the plugin/format support per-channel
    ///                       data formats?
    ///    "displaywindow"  Does the format support display ("full") windows
    ///                        distinct from the pixel data window?
    ///    "origin"         Does the format support a nonzero x,y,z
    ///                        origin of the pixel data window?
    ///    "negativeorigin" Does the format support negative x,y,z
    ///                        and full_{x,y,z} origin values?
    ///    "deepdata"       Deep (multi-sample per pixel) data
    ///    "arbitrary_metadata" Does this format allow metadata with
    ///                        arbitrary names and types?
    ///    "exif"           Can this format store Exif camera data?
    ///    "iptc"           Can this format store IPTC data?
    ///    "ioproxy"        Does this format writer support IOProxy?
    ///
    /// Note that main advantage of this approach, versus having
    /// separate individual supports_foo() methods, is that this allows
    /// future expansion of the set of possible queries without changing
    /// the API, adding new entry points, or breaking linkage
    /// compatibility.
    virtual int supports (string_view feature) const { return false; }

    enum OpenMode { Create, AppendSubimage, AppendMIPLevel };

    /// Open the file with given name, with resolution and other format
    /// data as given in newspec.  Open returns true for success, false
    /// for failure.  Note that it is legal to call open multiple times
    /// on the same file without a call to close(), if it supports
    /// multiimage and mode is AppendSubimage, or if it supports
    /// MIP-maps and mode is AppendMIPLevel -- this is interpreted as
    /// appending a subimage, or a MIP level to the current subimage,
    /// respectively.
    virtual bool open (const std::string &name, const ImageSpec &newspec,
                       OpenMode mode=Create) = 0;

    /// Open the file with given name, expecting to have a given total
    /// number of subimages, described by specs[0..subimages-1].  
    /// Return true for success, false for failure.  Upon success, the
    /// first subimage will be open and ready for transmission of
    /// pixels.  Subsequent subimages will be denoted with the usual
    /// call of open(name,spec,AppendSubimage) (and MIP levels by
    /// open(name,spec,AppendMIPLevel)).
    ///
    /// The purpose of this call is to accommodate format-writing
    /// libraries that fmust know the number and specifications of the
    /// subimages upon first opening the file; such formats can be
    /// detected by
    ///     supports("multiimage") && !supports("appendsubimage")
    /// The individual specs passed to the appending open() calls for
    /// subsequent subimages MUST match the ones originally passed.
    virtual bool open (const std::string &name, int subimages,
                       const ImageSpec *specs) {
        // Default implementation: just a regular open, assume that
        // appending will work.
        return open (name, specs[0]);
    }

    /// Return a reference to the image format specification of the
    /// current subimage.  Note that the contents of the spec are
    /// invalid before open() or after close().
    const ImageSpec &spec (void) const { return m_spec; }

    /// Close an image that we are totally done with.  This should leave
    /// the plugin in a state where it could open a new file safely,
    /// without having to destroy the writer.
    virtual bool close () = 0;

    /// Write a full scanline that includes pixels (*,y,z).  (z is
    /// ignored for 2D non-volume images.)  The stride value gives the
    /// distance between successive pixels (in bytes).  Strides set to
    /// AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    /// The data are automatically converted from 'format' to the actual
    /// output format (as specified to open()) by this method.  
    /// If format is TypeDesc::UNKNOWN, then rather than converting from
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// Return true for success, false for failure.  It is a failure to
    /// call write_scanline with an out-of-order scanline if this format
    /// driver does not support random access.
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride=AutoStride);

    /// Write multiple scanlines that include pixels (*,y,z) for all
    /// ybegin <= y < yend, from data.  This is analogous to
    /// write_scanline except that it may be used to write more than one
    /// scanline at a time (which, for some formats, may be able to be
    /// done much more efficiently or in parallel).
    virtual bool write_scanlines (int ybegin, int yend, int z,
                                  TypeDesc format, const void *data,
                                  stride_t xstride=AutoStride,
                                  stride_t ystride=AutoStride);

    /// Write the tile with (x,y,z) as the upper left corner.  (z is
    /// ignored for 2D non-volume images.)  The three stride values give
    /// the distance (in bytes) between successive pixels, scanlines,
    /// and volumetric slices, respectively.  Strides set to AutoStride
    /// imply 'contiguous' data in the shape of a full tile, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.tile_width
    ///     zstride == ystride*spec.tile_height
    /// The data are automatically converted from 'format' to the actual
    /// output format (as specified to open()) by this method.  
    /// If format is TypeDesc::UNKNOWN, then rather than converting from
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// Return true for success, false for failure.  It is a failure to
    /// call write_tile with an out-of-order tile if this format driver
    /// does not support random access.
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride);

    /// Write the block of multiple tiles that include all pixels in
    /// [xbegin,xend) X [ybegin,yend) X [zbegin,zend).  This is analogous to
    /// write_tile except that it may be used to write more than one tile at
    /// a time (which, for some formats, may be able to be done much more
    /// efficiently or in parallel).
    /// The begin/end pairs must correctly delineate tile boundaries, with
    /// the exception that it may also be the end of the image data if the
    /// image resolution is not a whole multiple of the tile size.
    /// The stride values give the data spacing of adjacent pixels,
    /// scanlines, and volumetric slices (measured in bytes). Strides set to
    /// AutoStride imply 'contiguous' data in the shape of the [begin,end)
    /// region, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride * (xend-xbegin)
    ///     zstride == ystride * (yend-ybegin)
    virtual bool write_tiles (int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend, TypeDesc format,
                              const void *data, stride_t xstride=AutoStride,
                              stride_t ystride=AutoStride,
                              stride_t zstride=AutoStride);

    /// Write a rectangle of pixels given by the range
    ///   [xbegin,xend) X [ybegin,yend) X [zbegin,zend)
    /// The stride values give the data spacing of adjacent pixels,
    /// scanlines, and volumetric slices (measured in bytes). Strides set to
    /// AutoStride imply 'contiguous' data in the shape of the [begin,end)
    /// region, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride * (xend-xbegin)
    ///     zstride == ystride * (yend-ybegin)
    /// The data are automatically converted from 'format' to the actual
    /// output format (as specified to open()) by this method.  If
    /// format is TypeDesc::UNKNOWN, it will just copy pixels assuming
    /// they are already in the file's native data layout (including,
    /// possibly, per-channel data formats).
    ///
    /// Return true for success, false for failure.  It is a failure to
    /// call write_rectangle for a format plugin that does not return
    /// true for supports("rectangles").
    virtual bool write_rectangle (int xbegin, int xend, int ybegin, int yend,
                                  int zbegin, int zend, TypeDesc format,
                                  const void *data, stride_t xstride=AutoStride,
                                  stride_t ystride=AutoStride,
                                  stride_t zstride=AutoStride);

    /// Write the entire image of spec.width x spec.height x spec.depth
    /// pixels, with the given strides and in the desired format.
    /// Strides set to AutoStride imply 'contiguous' data, i.e.,
    ///     xstride == spec.nchannels*format.size()
    ///     ystride == xstride*spec.width
    ///     zstride == ystride*spec.height
    /// Depending on spec, write either all tiles or all scanlines.
    /// Assume that data points to a layout in row-major order.
    /// If format is TypeDesc::UNKNOWN, then rather than converting from
    /// format, it will just copy pixels in the file's native data layout
    /// (including, possibly, per-channel data formats).
    /// Because this may be an expensive operation, a progress callback
    /// may be passed.  Periodically, it will be called as follows:
    ///   progress_callback (progress_callback_data, float done)
    /// where 'done' gives the portion of the image
    virtual bool write_image (TypeDesc format, const void *data,
                              stride_t xstride=AutoStride,
                              stride_t ystride=AutoStride,
                              stride_t zstride=AutoStride,
                              ProgressCallback progress_callback=NULL,
                              void *progress_callback_data=NULL);

    /// Write deep scanlines containing pixels (*,y,z), for all y in
    /// [ybegin,yend), to a deep file.
    virtual bool write_deep_scanlines (int ybegin, int yend, int z,
                                       const DeepData &deepdata);

    /// Write the block of deep tiles that include all pixels in
    /// [xbegin,xend) X [ybegin,yend) X [zbegin,zend).  
    /// The begin/end pairs must correctly delineate tile boundaries,
    /// with the exception that it may also be the end of the image data
    /// if the image resolution is not a whole multiple of the tile size.
    virtual bool write_deep_tiles (int xbegin, int xend, int ybegin, int yend,
                                   int zbegin, int zend,
                                   const DeepData &deepdata);

    /// Write the entire deep image denoted by data.
    virtual bool write_deep_image (const DeepData &deepdata);

    /// Read the current subimage of 'in', and write it as the next
    /// subimage of *this, in a way that is efficient and does not alter
    /// pixel values, if at all possible.  Both in and this must be a
    /// properly-opened ImageInput and ImageOutput, respectively, and
    /// their current images must match in size and number of channels.
    /// Return true if it works ok, false if for some reason the
    /// operation wasn't possible.
    ///
    /// If a particular ImageOutput implementation does not supply a
    /// copy_image method, it will inherit the default implementation,
    /// which is to simply read scanlines or tiles from 'in' and write
    /// them to *this.  However, some ImageIO implementations may have a
    /// special technique for directly copying raw pixel data from the
    /// input to the output, when both input and output are the SAME
    /// file type and the same data format.  This can be more efficient
    /// than in->read_image followed by out->write_image, and avoids any
    /// unintended pixel alterations, especially for formats that use
    /// lossy compression.
    virtual bool copy_image (ImageInput *in);

    /// General message passing between client and image output server
    ///
    virtual int send_to_output (const char *format, ...);
    int send_to_client (const char *format, ...);

    /// If any of the API routines returned false indicating an error,
    /// this routine will return the error string (and clear any error
    /// flags).  If no error has occurred since the last time geterror()
    /// was called, it will return an empty string.
    std::string geterror () const {
        std::string e = m_errmessage;
        m_errmessage.clear ();
        return e;
    }

    /// Error reporting for the plugin implementation: call this with
    /// Strutil::format-like arguments.
    /// Use with caution! Some day this will change to be fmt-like rather
    /// than printf-like.
    template<typename... Args>
    void error(const char* fmt, const Args&... args) const {
        append_error(Strutil::format (fmt, args...));
    }

    /// Error reporting for the plugin implementation: call this with
    /// printf-like arguments.
    template<typename... Args>
    void errorf(const char* fmt, const Args&... args) const {
        append_error(Strutil::sprintf (fmt, args...));
    }

    /// Error reporting for the plugin implementation: call this with
    /// fmt::format-like arguments.
    template<typename... Args>
    void fmterror(const char* fmt, const Args&... args) const {
        append_error(Strutil::fmt::format (fmt, args...));
    }

    /// Set the current thread-spawning policy: the maximum number of
    /// threads that may be spawned by ImageOutput internals. A value of 1
    /// means all work will be done by the calling thread; 0 means to use
    /// the global OIIO::attribute("threads") value.
    void threads (int n) { m_threads = n; }

    /// Retrieve the current thread-spawning policy.
    int threads () const { return m_threads; }

    // Custom new and delete to ensure that allocations & frees happen in
    // the OpenImageIO library, not in the app or plugins (because Windows).
    void* operator new (size_t size);
    void operator delete (void *ptr);

    /// Call signature of a function that creates and returns an ImageOutput*
    typedef ImageOutput* (*Creator)();

protected:
    /// Helper routines used by write_* implementations: convert data (in
    /// the given format and stride) to the "native" format of the file
    /// (described by the 'spec' member variable), in contiguous order. This
    /// requires a scratch space to be passed in so that there are no memory
    /// leaks.  Returns a pointer to the native data, which may be the
    /// original data if it was already in native format and contiguous, or
    /// it may point to the scratch space if it needed to make a copy or do
    /// conversions. For float->uint8 conversions only, if dither is
    /// nonzero, random dither will be added to reduce quantization banding
    /// artifacts; in this case, the specific nonzero dither value is used
    /// as a seed for the hash function that produces the per-pixel dither
    /// amounts, and the optional [xyz]origin parameters help it to align
    /// the pixels to the right position in the dither pattern.
    const void *to_native_scanline (TypeDesc format,
                                    const void *data, stride_t xstride,
                                    std::vector<unsigned char> &scratch,
                                    unsigned int dither=0,
                                    int yorigin=0, int zorigin=0);
    const void *to_native_tile (TypeDesc format, const void *data,
                                stride_t xstride, stride_t ystride,
                                stride_t zstride,
                                std::vector<unsigned char> &scratch,
                                unsigned int dither=0,
                                int xorigin=0, int yorigin=0, int zorigin=0);
    const void *to_native_rectangle (int xbegin, int xend, int ybegin, int yend,
                                     int zbegin, int zend,
                                     TypeDesc format, const void *data,
                                     stride_t xstride, stride_t ystride,
                                     stride_t zstride,
                                     std::vector<unsigned char> &scratch,
                                     unsigned int dither=0,
                                     int xorigin=0, int yorigin=0, int zorigin=0);

    /// Helper function to copy a rectangle of data into the right spot in
    /// an image-sized buffer. In addition to copying to the right place,
    /// this handles data format conversion and dither (if the spec's
    /// "oiio:dither" is nonzero, and if it's converting from a float-like
    /// type to UINT8). The buf_format describes the type of image_buffer,
    /// if it's TypeDesc::UNKNOWN it will be assumed to be spec.format.
    bool copy_to_image_buffer (int xbegin, int xend, int ybegin, int yend,
                               int zbegin, int zend, TypeDesc format,
                               const void *data, stride_t xstride,
                               stride_t ystride, stride_t zstride,
                               void *image_buffer,
                               TypeDesc buf_format = TypeDesc::UNKNOWN);
    /// Helper function to copy a tile of data into the right spot in an
    /// image-sized buffer. This is really just a wrapper for
    /// copy_to_image_buffer, passing all the right parameters to copy
    /// exactly one tile.
    bool copy_tile_to_image_buffer (int x, int y, int z, TypeDesc format,
                                    const void *data, stride_t xstride,
                                    stride_t ystride, stride_t zstride,
                                    void *image_buffer,
                                    TypeDesc buf_format = TypeDesc::UNKNOWN);

protected:
    ImageSpec m_spec;           ///< format spec of the currently open image

private:
    void append_error (const std::string& message) const; // add to m_errmessage
    mutable std::string m_errmessage;   ///< private storage of error message
    int m_threads;    // Thread policy
};



// Utility functions

/// Retrieve the version of OpenImageIO for the library.  This is so
/// plugins can query to be sure they are linked against an adequate
/// version of the library.
OIIO_API int openimageio_version ();

/// Special geterror() called after ImageInput::create or
/// ImageOutput::create, since if create fails, there's no object on
/// which call obj->geterror().  This function returns the last error
/// for this particular thread; separate threads will not clobber each
/// other's global error messages.
OIIO_API std::string geterror ();

/// Set a global attribute controlling OpenImageIO.  Return true
/// if the name and type were recognized and the attribute was set.
///
/// Documented attributes:
///     int threads
///             How many threads to use for operations that can be sped
///             by spawning threads (default=0, meaning to use the full
///             available hardware concurrency detected).
///     int exr_threads
///             The size of the internal OpenEXR thread pool. The default
///             is to use the full available hardware concurrency detected.
///             Default is 0 meaning to use full available hardware
///             concurrency detected, -1 means to disable usage of the OpenEXR
///             thread pool and execute everything in the caller thread.
///     string plugin_searchpath
///             Colon-separated list of directories to search for 
///             dynamically-loaded format plugins.
///     int read_chunk
///             The number of scanlines that will be attempted to read at
///             once for read_image calls (default: 256).
///     int debug
///             When nonzero, various debug messages may be printed.
///             The default is 0 for release builds, >=1 for DEBUG builds,
///             but also may be overridden by the OPENIMAGEIO_DEBUG env
///             variable.
///     int log_times
///             When nonzero, various internals will record how much total
///             time they spend in execution. If the value is >= 2, these
///             times will be printed upon exit. Thd default is 0, but will
///             be initialized to the value of the OPENIMAGEIO_LOG_TIMES
///             environment variable, if it exists.
///     int tiff:half
///             When nonzero, allows TIFF to write 'half' pixel data.
///             N.B. Most apps may not read these correctly, but OIIO will.
///             That's why the default is not to support it.
///
OIIO_API bool attribute (string_view name, TypeDesc type, const void *val);
// Shortcuts for common types
inline bool attribute (string_view name, int val) {
    return attribute (name, TypeInt, &val);
}
inline bool attribute (string_view name, float val) {
    return attribute (name, TypeFloat, &val);
}
inline bool attribute (string_view name, string_view val) {
    const char *s = val.c_str();
    return attribute (name, TypeString, &s);
}

/// Get the named global attribute of OpenImageIO, store it in *val.
/// Return true if found and it was compatible with the type specified,
/// otherwise return false and do not modify the contents of *val.  It
/// is up to the caller to ensure that val points to the right kind and
/// size of storage for the given type.
///
/// In addition to being able to retrieve all the attributes that are
/// documented as settable by the attribute() call, getattribute() can
/// also retrieve the following read-only attributes:
///     string "format_list"
///             Comma-separated list of all format names supported
///             or for which plugins could be found.
///     string "input_format_list"
///             Comma-separated list of all format names supported
///             or for which plugins could be found that can read images.
///     string "output_format_list"
///             Comma-separated list of all format names supported
///             or for which plugins could be found that can write images.
///     string "extension_list"
///             For each format, the format name followed by a colon,
///             followed by comma-separated list of all extensions that
///             are presumed to be used for that format.  Semicolons
///             separate the lists for formats.  For example,
///                "tiff:tif;jpeg:jpg,jpeg;openexr:exr"
///     string "library_list"
///             For each format that uses an external expendent library, the
///             format name followed by a colon, followed by the name of
///             the library. Semicolons separate the lists for formats. For
///             example,
///              "jpeg:jpeg-turbo 1.5.1;png:libpng 1.6.29;gif:gif_lib 5.1.4"
///     string "timing_report"
///             A string containing the report of all the log_times.
///     string "oiio:simd"
///             Comma-separated list of the SIMD-related capabilities
///             enabled when the OIIO library was built. For example,
///                 "sse2,sse3,ssse3,sse41,sse42,avx"
///     string "hw:simd"
///             Comma-separated list of the SIMD-related capabilities
///             detected at runtime at the time of the query (which may not
///             match the support compiled into the library).
///     int "resident_memory_used_MB"
///             Approximate process memory used (resident) by the application,
///             in MB. This might be helpful in debugging.
OIIO_API bool getattribute (string_view name, TypeDesc type, void *val);
// Shortcuts for common types
inline bool getattribute (string_view name, int &val) {
    return getattribute (name, TypeInt, &val);
}
inline bool getattribute (string_view name, float &val) {
    return getattribute (name, TypeFloat, &val);
}
inline bool getattribute (string_view name, char **val) {
    return getattribute (name, TypeString, val);
}
inline bool getattribute (string_view name, std::string &val) {
    ustring s;
    bool ok = getattribute (name, TypeString, &s);
    if (ok)
        val = s.string();
    return ok;
}
inline int get_int_attribute (string_view name, int defaultval=0) {
    int val;
    return getattribute (name, TypeInt, &val) ? val : defaultval;
}
inline float get_float_attribute (string_view name, float defaultval=0) {
    float val;
    return getattribute (name, TypeFloat, &val) ? val : defaultval;
}
inline string_view get_string_attribute (string_view name,
                                 string_view defaultval = string_view()) {
    ustring val;
    return getattribute (name, TypeString, &val) ? string_view(val) : defaultval;
}


/// Register the input and output 'create' routines and list of file
/// extensions for a particular format.
OIIO_API void declare_imageio_format (const std::string &format_name,
                                      ImageInput::Creator input_creator,
                                      const char **input_extensions,
                                      ImageOutput::Creator output_creator,
                                      const char **output_extensions,
                                      const char *lib_version);

/// Helper function: convert contiguous arbitrary data between two
/// arbitrary types (specified by TypeDesc's)
/// Return true if ok, false if it didn't know how to do the
/// conversion.  If dst_type is UNKNOWN, it will be assumed to be the
/// same as src_type.
OIIO_API bool convert_types (TypeDesc src_type, const void *src,
                              TypeDesc dst_type, void *dst, int n);

/// Helper routine for data conversion: Convert an image of nchannels x
/// width x height x depth from src to dst.  The src and dst may have
/// different data formats and layouts.  Clever use of this function can
/// not only exchange data among different formats (e.g., half to 8-bit
/// unsigned), but also can copy selective channels, copy subimages,
/// etc.  If you're lazy, it's ok to pass AutoStride for any of the
/// stride values, and they will be auto-computed assuming contiguous
/// data.  Return true if ok, false if it didn't know how to do the
/// conversion.
OIIO_API bool convert_image (int nchannels, int width, int height, int depth,
                             const void *src, TypeDesc src_type,
                             stride_t src_xstride, stride_t src_ystride,
                             stride_t src_zstride,
                             void *dst, TypeDesc dst_type,
                             stride_t dst_xstride, stride_t dst_ystride,
                             stride_t dst_zstride);
/// DEPRECATED(2.0) -- the alpha_channel, z_channel were never used
inline bool convert_image(int nchannels, int width, int height, int depth,
            const void *src, TypeDesc src_type,
            stride_t src_xstride, stride_t src_ystride, stride_t src_zstride,
            void *dst, TypeDesc dst_type,
            stride_t dst_xstride, stride_t dst_ystride, stride_t dst_zstride,
            int alpha_channel, int z_channel = -1)
{
    return convert_image(nchannels, width, height, depth, src, src_type,
                         src_xstride, src_ystride, src_zstride, dst, dst_type,
                         dst_xstride, dst_ystride, dst_zstride);
}


/// A version of convert_image that will break up big jobs into multiple
/// threads.
OIIO_API bool parallel_convert_image (
               int nchannels, int width, int height, int depth,
               const void *src, TypeDesc src_type,
               stride_t src_xstride, stride_t src_ystride,
               stride_t src_zstride,
               void *dst, TypeDesc dst_type,
               stride_t dst_xstride, stride_t dst_ystride,
               stride_t dst_zstride, int nthreads=0);
/// DEPRECATED(2.0) -- the alpha_channel, z_channel were never used
inline bool parallel_convert_image(
            int nchannels, int width, int height, int depth,
            const void *src, TypeDesc src_type,
            stride_t src_xstride, stride_t src_ystride, stride_t src_zstride,
            void *dst, TypeDesc dst_type,
            stride_t dst_xstride, stride_t dst_ystride, stride_t dst_zstride,
            int alpha_channel, int z_channel, int nthreads=0)
{
    return parallel_convert_image (nchannels, width, height, depth,
           src, src_type, src_xstride, src_ystride, src_zstride,
           dst, dst_type, dst_xstride, dst_ystride, dst_zstride, nthreads);
}

/// Add random [-theramplitude,ditheramplitude] dither to the color channels
/// of the image.  Dither will not be added to the alpha or z channel.  The
/// image origin and dither seed values allow a reproducible (or variable)
/// dither pattern.  If the strides are set to AutoStride, they will be
/// assumed to be contiguous floats in data of the given dimensions.
OIIO_API void add_dither (int nchannels, int width, int height, int depth,
                          float *data,
                          stride_t xstride, stride_t ystride, stride_t zstride,
                          float ditheramplitude,
                          int alpha_channel = -1, int z_channel = -1,
                          unsigned int ditherseed = 1,
                          int chorigin=0, int xorigin=0,
                          int yorigin=0, int zorigin=0);

/// Convert unassociated to associated alpha by premultiplying all color
/// (non-alpha, non-z) channels by alpha.
OIIO_API void premult (int nchannels, int width, int height, int depth,
                       int chbegin, int chend,
                       TypeDesc datatype, void *data, stride_t xstride,
                       stride_t ystride, stride_t zstride,
                       int alpha_channel = -1, int z_channel = -1);

/// Helper routine for data conversion: Copy an image of nchannels x
/// width x height x depth from src to dst.  The src and dst may have
/// different data layouts, but must have the same data type.  Clever
/// use of this function can change layouts or strides, copy selective
/// channels, copy subimages, etc.  If you're lazy, it's ok to pass
/// AutoStride for any of the stride values, and they will be
/// auto-computed assuming contiguous data.  Return true if ok, false if
/// it didn't know how to do the conversion.
OIIO_API bool copy_image (int nchannels, int width, int height, int depth,
                          const void *src, stride_t pixelsize,
                          stride_t src_xstride, stride_t src_ystride,
                          stride_t src_zstride,
                          void *dst, stride_t dst_xstride,
                          stride_t dst_ystride, stride_t dst_zstride);


// All the wrap_foo functions implement a wrap mode, wherein coord is
// altered to be origin <= coord < origin+width.  The return value
// indicates if the resulting wrapped value is valid (example, for
// wrap_black, values outside the region are invalid and do not modify
// the coord parameter).
OIIO_API bool wrap_black (int &coord, int origin, int width);
OIIO_API bool wrap_clamp (int &coord, int origin, int width);
OIIO_API bool wrap_periodic (int &coord, int origin, int width);
OIIO_API bool wrap_periodic_pow2 (int &coord, int origin, int width);
OIIO_API bool wrap_mirror (int &coord, int origin, int width);

// Typedef for the function signature of a wrap implementation.
typedef bool (*wrap_impl) (int &coord, int origin, int width);


/// debug(format, ...) prints debugging message when attribute "debug" is
/// nonzero, which it is by default for DEBUG compiles or when the
/// environment variable OPENIMAGEIO_DEBUG is set. This is preferred to raw
/// output to stderr for debugging statements.
OIIO_API void debug (string_view str);

/// debug output with fmt/std::format conventions.
template<typename T1, typename... Args>
void fmtdebug (const char* fmt, const T1& v1, const Args&... args)
{
    debug (Strutil::fmt::format(fmt, v1, args...));
}

/// debug output with printf conventions.
template<typename T1, typename... Args>
void debugf (const char* fmt, const T1& v1, const Args&... args)
{
    debug (Strutil::sprintf(fmt, v1, args...));
}

/// debug output with the same conventions as Strutil::format. Beware, this
/// will change one day!
template<typename T1, typename... Args>
void debug (const char* fmt, const T1& v1, const Args&... args)
{
    debug (Strutil::format(fmt, v1, args...));
}


// to force correct linkage on some systems
OIIO_API void _ImageIO_force_link ();

OIIO_NAMESPACE_END