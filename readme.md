# DirectShow VANC 608 Parsing Filter

This is a DirectShow Graph Filter that accepts a raw video stream /w VANC data, extracts 608 captions and converts them into a standard Line21 output source. 

I developed this filter MANY years ago and now available for curiosity. 

The magic of this filter is contained with VANCParser which parses the actual line
21 signal packet on each raw video frame.  
