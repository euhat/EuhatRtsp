#ifndef Unit_Read_SPS_h__
#define Unit_Read_SPS_h__
#include <stdio.h>
//typedef char int8_t;

typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
//typedef __int64 int64_t;
//typedef unsigned __int64 uint64_t;

//for H264

typedef struct tag_bs_s
{
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    int     i_left;			// i_count number of available bits
    int     i_bits_encoded; // RD only
} bs_t;


class SPSParser
{
public:
    SPSParser();
    ~SPSParser();

public:
     int	 Do_Read_SPS( bs_t *s, int *width, int *height);

private:
    int			_bs_eof( bs_t *s );
    uint32_t	_bs_read( bs_t *s, int i_count );
    void		_bs_skip( bs_t *s, int i_count );
    int			_bs_read_ue( bs_t *s );
    int			_bs_read_se( bs_t *s );
    uint32_t    _bs_read1( bs_t *s );
    void		_scaling_list(bs_t *s,int scalingList,int sizeOfScalingList,int useDefaultScalingMatrixFlag ) ;
};

#endif  //Unit_Read_SPS_h__
