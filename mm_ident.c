
/*********************  P r o g r a m  -  M o d u l e **********************/
/*!
 *         \file mm_ident.c
 *      Project: native linux M-Module ident tool
 *
 *       \author awe
 *
 *        \brief Tool to read the M-Module EEPROM.
 *               This is tool is a standalone version of the id library-
 *
 *
 *
 *---------------------------------------------------------------------------
 * Copyright 2014-2019, MEN Mikro Elektronik GmbH
 ****************************************************************************/

 /*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define DELAY   20	                /* m_clock's delay time */

/* id defines */
#define MOD_ID_MAGIC	0x5346  	/* M-Module id prom magic word */
#define MOD_ID_MS_MASK	0x5300		/* mask to indicate MSxx M-Module */
#define MOD_ID_N_MASK	0x7D00		/* mask to indicate MxxN M-Module */

/*--- instructions for serial EEPROM ---*/
#define     _READ_   0x80    		/* read data */
#define     EWEN     0x30    		/* enable erase/write state */
#define     ERASE    0xc0    		/* erase cell */
#define     _WRITE_  0x40    		/* write data */
#define     ERAL     0x20    		/* chip erase */
#define     WRAL     0x10    		/* chip write */
#define     EWDS     0x00    		/* disable erase/write state */

#define     T_WP    10000   		/* max. time required for write/erase (us) */

/* bit definition */
#define B_DAT	0x01			/* data in-;output */
#define B_CLK	0x02			/* clock */
#define B_SEL	0x04			/* chip-select */

/* A08 register address */
#define     MODREG  0xfe

#define FALSE 0x00
#define TRUE 0x01

#define MODCOM_MOD_MEN 1
#define MODCOM_MOD_THIRD 2

#ifndef MAP_32BIT
#define MAP_32BIT 0x40			/* only give out 32bit addresses */
#endif

/*--- K&R prototypes ---*/
static int _write( uint32_t base, uint8_t index, uint16_t data );
static int _erase( uint32_t base, uint8_t index );
static void _opcode( uint32_t base, uint8_t code );
static void _select( uint32_t base );
static void _deselect( uint32_t base );
static int _clock( uint32_t base, uint8_t dbs );
static void _delay( void );
static void _xtoa( uint32_t val, uint32_t radix, char *buf );
static void MWRITE_D16(uint32_t base, uint32_t offset, uint16_t val);
static uint16_t MREAD_D16(uint32_t base, uint32_t offset);
int m_read( uint32_t base, uint8_t index );
int m_write( uint8_t *addr, uint8_t  index, uint16_t data );

/******************************* _xtoa *************************************/
/**   Converts an u_int32 to a character string.
 *
 *---------------------------------------------------------------------------
 *  \param radix		\IN	base to convert into
 *  \param val			\IN	number to be converted
 *  \param buf			\IN	ptr to buffer to place result
 *	\param buf			\OUT computed string
 *  Globals....:  ---
 ****************************************************************************/
static void _xtoa( uint32_t val, uint32_t radix, char *buf )
{
	char	*p;           /* pointer to traverse string */
    char	*firstdig;    /* pointer to first digit */
    char	temp;         /* temp char */
    uint32_t digval;       /* value of digit */

    p = buf;

	/* save pointer to first digit */
    firstdig = p;

    do {
        digval = (uint32_t) (val % radix);
        val /= radix; /* get next digit */

        /* convert to ascii and store */
        if (digval > 9)
            *p++ = (char) (digval - 10 + 'a');  /* a letter */
        else
            *p++ = (char) (digval + '0');       /* a digit */
    } while (val > 0);

    /* terminate string; p points to last digit */
    *p-- = '\0';

    /* reverse buffer */
    do {
        temp = *p;
        *p = *firstdig;
        *firstdig = temp;   /* swap *p and *firstdig */
        --p;
        ++firstdig;         /* advance to next two digits */
    } while (firstdig < p); /* repeat until halfway */
}

/******************************* _write ***********************************/
/**   Write a specified word into EEPROM at 'base'.
 *
 *---------------------------------------------------------------------------
 *	\param base			\IN base address pointer
 *	\param index		\IN index to write (0..63)
 *  \param data			\IN word to write
 *  \return   0=ok 1=write err 2=verify err
 *
 ***************************************************************************/
static int _write( uint32_t base, uint8_t index, uint16_t data )
{
    register int    i,j;                    /* counters     */

    _opcode(base,EWEN);                     /* write enable */
    _deselect(base);                        /* deselect     */

    _opcode(base, (uint8_t)(_WRITE_+index) );             /* select write */
    for(i=15; i>=0; i--)
        _clock(base,(uint8_t)((data>>i)&0x01));        /* write data   */
    _deselect(base);                        /* deselect     */

    _select(base);
    for(i=T_WP; i>0; i--)                   /* wait for low */
    {   if(!_clock(base,0))
            break;
        _delay();
    }
    for(j=T_WP; j>0; j--)                   /* wait for high*/
    {   if(_clock(base,0))
            break;
        _delay();
    }

    _opcode(base, EWDS);                    /* write disable*/
    _deselect(base);                        /* disable      */

    if((i==0) || (j==0))                    /* error ?      */
        return 1;                           /* ..yes */

    if( data != m_read(base,index) )        /* verify data  */
        return 2;                           /* ..error      */

    return 0;                               /* ..no         */
}

/******************************* _erase ***********************************/
/**   Erase a specified word into EEPROM
 *
 *---------------------------------------------------------------------------
 *	\param base			\IN base address pointer
 *	\param index		\IN index to write (0..15)
 *  \return   0=ok 1=error
 *
 ***************************************************************************/
static int _erase( uint32_t base, uint8_t index )
{
    register int    i,j;                    /* counters     */

    _opcode(base,EWEN);                     /* erase enable */
    for(i=0;i<4;i++) _clock(base,0);
    _deselect(base);                        /* deselect     */

    _opcode(base,(uint8_t)(ERASE+index) );              /* select erase */
    _deselect(base);                        /* deselect     */

    _select(base);
    for(i=T_WP; i>0; i--)                   /* wait for low */
    {   if(!_clock(base,0))
            break;
        _delay();
    }

    for(j=T_WP; j>0; j--)                   /* wait for high*/
    {   if(_clock(base,0))
            break;
        _delay();
    }

    _opcode(base,EWDS);                     /* erase disable*/
    _deselect(base);                        /* disable      */

    if((i==0) || (j==0))                    /* error ?      */
        return 1;
    return 0;
}

/******************************* _opcode ***********************************/
/**   Output opcode with leading startbit
 *
 *---------------------------------------------------------------------------
 *	\param base			\IN base address pointer
 *	\param code			\IN opcode to write
 *
 ***************************************************************************/
static void _opcode( uint32_t base, uint8_t code )
{
    register int i;
    _select(base);
    _clock(base,1);                         /* output start bit */

    for(i=7; i>=0; i--)
        _clock(base,(uint8_t)((code>>i)&0x01) );        /* output instruction code  */
}

/******************************* _select ***********************************/
/**   Select EEPROM:
 *                 output DI/CLK/CS low
 *                 delay
 *                 output CS high
 *                 delay
 *---------------------------------------------------------------------------
 *  \param base			\IN base address pointer
 *
 ***************************************************************************/
static void _select( uint32_t base )
{
    MWRITE_D16( base, MODREG, 0 );			/* everything inactive */
    MWRITE_D16( base, MODREG, B_SEL );		/* select high */
    _delay();
}

/******************************* _deselect *********************************/
/**   Deselect EEPROM
 *                 output CS low
 *---------------------------------------------------------------------------
 *  \param base			\IN base address pointer
 *
 ***************************************************************************/
static void _deselect( uint32_t base )
{
    MWRITE_D16( base, MODREG, 0 );			/* everything inactive */
}


/******************************* MWRITE_D16 *********************************/
/**   Write 16bit value to a memory address.
 *---------------------------------------------------------------------------
 *  \param base			\IN base address pointer
 *  \param offset		\IN memory offset
 *  \param val			\IN value to write to the memory are
 *
 ***************************************************************************/
static void MWRITE_D16(uint32_t base, uint32_t offset, uint16_t val)
{
	uint32_t *addr;
	
	addr = (uint32_t*)(uintptr_t)(base + offset);
	*addr = val;

}

/******************************* MREAD_D16 *********************************/
/**   Read a 16bit value from a memory address.
 *---------------------------------------------------------------------------
 *  \param base			\IN base address pointer
 *  \param offset		\IN memory offset
 * 
 *  \return value read from the memory address
 ***************************************************************************/
static uint16_t MREAD_D16(uint32_t base, uint32_t offset)
{
	uint32_t *value;

	value = (uint32_t*)(uintptr_t)(base + offset);
	return (uint16_t)*value;
}

/******************************* _clock ***********************************/
/**   Output data bit:
 *                 output clock low
 *                 output data bit
 *                 delay
 *                 output clock high
 *                 delay
 *                 return state of data serial eeprom's DO - line
 *                 (Note: keep CS asserted)
 *---------------------------------------------------------------------------
 *  \param base			\IN base address pointer
 *	\param dbs			\IN	data bit to send
 *  \return state of DO line
 *
 ***************************************************************************/
static int _clock( uint32_t base, uint8_t dbs )
{
    MWRITE_D16( base, MODREG, dbs|B_SEL );  /* output clock low */
                                            /* output data high/low */
    _delay();                               /* delay    */

    MWRITE_D16( base, MODREG, dbs|B_CLK|B_SEL );  /* output clock high */
    _delay();                               /* delay    */

    return( MREAD_D16( base, MODREG) & B_DAT );  /* get data */
}

/******************************* _delay ************************************/
/**   Delay (at least) one microsecond
 *---------------------------------------------------------------------------
 *
 ***************************************************************************/
static void _delay( void )
{
    register volatile int i,n;

    for(i=DELAY; i>0; i--)
        n=10*10;

    (void)n;
}

/******************************* m_mread ***********************************/
/**   Read all contents (words 0..15) from EEPROM at 'base'.
 *
 *---------------------------------------------------------------------------
 *  \param addr			\IN base address pointer
 *  \param buff			\INOUT user buffer (16 words)
 *  \return   0=ok, 1=error
 *
 ****************************************************************************/
int m_mread( uint8_t *addr, uint16_t  *buff )
{

    register uint8_t    index;

    for(index=0; index<16; index++)
        *buff++ = (uint8_t)m_read( (uint32_t)(uintptr_t)addr, index);
    return 0;
}

/******************************* m_mwrite **********************************/
/**   Write all contents (words 0..15) into EEPROM at 'base'.
 *
 *---------------------------------------------------------------------------
 *  \param addr		\IN base address pointer
 *  \param buff		\IN user buffer (16 words)
 *  \return   0=ok, 1=error
 *
 ****************************************************************************/
int m_mwrite( uint8_t *addr, uint8_t *buff) 
{
    register uint8_t    index;

    for(index=0; index<16; index++)
        if( m_write(addr,index,*buff++) )
            return 1;
    return 0;
}

/******************************* m_write ***********************************/
/**   Write a specified word into EEPROM at 'base'.
 *
 *---------------------------------------------------------------------------
 *  \param addr			\IN base address pointer
 *  \param index		\IN index to write (0..15)
 *  \param data			\IN word to write
 *  \return   0=ok; 1=write err; 2=verify err
 *
 ***************************************************************************/
int m_write( uint8_t *addr, uint8_t  index, uint16_t data )
{
    if( _erase( (uint32_t)(uintptr_t)addr, index ))              /* erase cell first */
        return 3;

    return _write( (uint32_t)(uintptr_t)addr, index, data );
}

/******************************* m_read ************************************/
/**   Read a specified word from EEPROM at 'base'.
 *
 *---------------------------------------------------------------------------
 *  \param base			\IN base address pointer
 *  \param index		\IN index to read (0..15)
 *  \return   read word
 *
 ****************************************************************************/
int m_read( uint32_t base, uint8_t index )
{
    register uint16_t    wx;                 /* data word    */
    register int        i;                  /* counter      */

    _opcode(base, (uint8_t)(_READ_+index) );
    for(wx=0, i=0; i<16; i++)
        wx = (uint16_t)((wx<<1)+_clock(base,0));
    _deselect(base);

    return(wx);
}

/******************************* m_getmodinfo ******************************/
/**   Get module information.
 *
 *                The function reads the magic-id, mod-id, layout-rev and
 *                product-variant from the EEPROM, evaluates these parameters
 *                and provide the module information for the caller.
 *
 *                1) If the four read values are equal, then we assume that
 *                   the EEPROM is not present or is invalid.
 *                   In this case, the function returns with the following
 *                   parameters:
 *                   - modtype = 0.
 *             		 - devid  = 0xffffffff
 *                   - devrev = 0xffffffff
 *                   - devname = '\\0'
 *
 *                2) If the four read values are different, the function
 *                   gives the following parameters to the caller:
 *             		 - devid  = (magic-id   << 16) | mod-id
 *                   - devrev = (layout-rev << 16) | product-variant
 *
 *                2a) If magic-id = 0x5346 the function returns with:
 *                    - modtype = MODCOM_MOD_MEN
 *					  - devname = "<prefix><decimal mod-id><suffix>"
 *					              - if (mod-id & 0xFF00) == 0x5300 then
 *                                  - prefix="MS"
 *					              - else
 *                                  - prefix="M"
 *					                - if (mod-id & 0xFF00) = 0x7D00 then
 *                                    - suffix="N"
 *
 *                                e.g. M34, MS9, M45N
 *
 *                2b) If magic-id <> 0x5346 the function returns with:
 *                  - modtype = MODCOM_MOD_THIRD
 *                  - devname = '\\0'
 *
 *---------------------------------------------------------------------------
 *  \param base			\IN	base address pointer
 *  \param modtype		\OUT module type (0, MODCOM_MOD_MEN, MODCOM_MOD_THIRD)
 *  \param devid		\OUT device id
 *  \param devrev		\OUT device revision
 *  \param devname		\OUT device name
 *  \return    0=ok, 1=error
 *
 ****************************************************************************/
int m_getmodinfo(
	uint32_t base,
	uint32_t *modtype,
	uint32_t *devid,
	uint32_t *devrev,
	char    *devname )
{
	uint16_t magic, modid, layout, variant;
	uint8_t	addSuffix = FALSE;
	char	*bufptr = devname;

	/* set defaults */
	*devid   = 0xffffffff;
	*devrev  = 0xffffffff;
	*devname = '\0';

	/* read data from eeprom */
	magic   = (uint16_t)m_read(base, 0);
	modid   = (uint16_t)m_read(base, 1);
	layout  = (uint16_t)m_read(base, 2);
	variant	= (uint16_t)m_read(base, 8);
	
	printf("MAGIC: 0x%x\n",magic);
	/*------------------------------+
	| M-Module without id-prom data |
	+------------------------------*/
	/*
	 * If all read data are equal then we assume there is a M-Module
	 * without id-prom or without valid id-prom data.
	 */
	if( (magic  == modid) &&
		(layout == variant) &&
		(magic  == layout) ){

		*modtype = 0;
		return 0;
	}

	/*------------------------------+
	| M-Module with id-prom data    |
	+------------------------------*/
	else {

		/* build devid and devrev */
		*devid   = (magic  << 16) | modid;
		*devrev  = (layout << 16) | variant;

		/*------------------------------+
		| VITA conform M-Module         |
		+------------------------------*/
		/*
		 * If we got the right magic-id then
		 * we assume there is a VITA conform M-Module.
		 */
		if( magic == MOD_ID_MAGIC ){

			*modtype = MODCOM_MOD_MEN;

			/*
			 * build device name
			 */

			*bufptr = 'M';
			bufptr++;

			/* MSxx M-Module? */
			if( (modid & 0xFF00) == MOD_ID_MS_MASK ){
				*bufptr = 'S';
				bufptr++;
				modid &= 0x00FF;
			}
			/* MxxN M-Module? */
			else if( (modid & 0xFF00) == MOD_ID_N_MASK ){
				addSuffix = TRUE;
				modid &= 0x00FF;
			}

			/* add modid */
			_xtoa( modid, 10, bufptr );

			/* MxxN M-Module */
			if( addSuffix ){
				bufptr = devname;
				while( *bufptr != '\0' )
					bufptr++;
				*bufptr++ = 'N';
				*bufptr = '\0';
			}
		}

		/*------------------------------+
		| other M-Module                |
		+------------------------------*/
		/*
		 * Not the right magic-id
		 */
		else{
			*modtype = MODCOM_MOD_THIRD;
		}
	}

	return 0;
}


void usage()
{
	printf("--------------------------------------------\n");
	printf("mm_ident <addr>				    \n");
	printf("  <addr> - MM-Module Addresse (BAR + Offset)\n");
	printf("--------------------------------------------\n");
}


/******************************* main ************************************/
/**   Map the M-Module memory and print the id informations
 *
 *---------------------------------------------------------------------------
 *  \param argc			\IN Argument Counter
 *  \param argb			\IN Argument list
 *  \return   0 on success 1 on error
 *
 ****************************************************************************/
int main(int argc, char** argv)
{
        int fd;
        uint32_t *vmem;
	uint32_t phys_addr;
	uint32_t modtype, devid, devrev;
	uint32_t pagesize, pageaddr;
	char devname[25];


	if (argc < 2) {
		usage();
		return 1;
	}

	sscanf(argv[1],"%x",&phys_addr);
	printf("PhysAddr: 0x%08x\n",phys_addr);
        
        fd = open("/dev/mem",O_RDWR|O_SYNC);
	if(fd < 0) {
                printf("Can't open /dev/mem\n");
                return 1;
        }

	/* mmap needs a page aligned address */
	pagesize = getpagesize();
	pageaddr = phys_addr & ~(pagesize-1);

	/* map always in the 32bit area this works for 32bit and 64bit */
	vmem = (uint32_t *) mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED | MAP_32BIT, fd, pageaddr);

	/* Add the page offset */
	vmem = (uint32_t*)(uintptr_t)((uint32_t)(uintptr_t)vmem | (phys_addr & (pagesize - 1)));
	
        if(vmem == NULL) {
                printf("Can't mmap memory reagion\n");
                return 1;
	}
	
	if (m_getmodinfo((uint32_t)(uintptr_t)vmem, &modtype, &devid, &devrev, devname))
		printf("Error reading modinfo\n");
	else
		printf("Type: 0x%04x, ID: 0x%04x, Rev: 0x%04x, Name: %s\n",
					modtype, (uint16_t)devid, (uint16_t)devrev,devname);

	munmap(vmem, getpagesize());

        return 0;
}



