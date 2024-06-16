# DirectShow VANC 608 Parsing Filter

This is a DirectShow Graph Filter that accepts a raw video stream with [SMPTE VANC](https://pub.smpte.org/pub/st334-1/st0334-1-2015.pdf) data, extracts 608 captions and converts them into a standard Line 21 2-byte output source. 

I developed this filter MANY years ago and now available for curiosity. 

The magic of this filter is contained with VANCParser which parses the actual line 21 signal packet on each raw video frame.  

