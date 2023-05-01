#include "oncsSub_idtpcfeev3.h"

#include <string.h>
#include <stdint.h>

#define coutfl cout << __FILE__<< "  " << __LINE__ << " "
#define cerrfl cerr << __FILE__<< "  " << __LINE__ << " "

using namespace std;

oncsSub_idtpcfeev3::oncsSub_idtpcfeev3(subevtdata_ptr data)
  :oncsSubevent_w2 (data)
{

 _is_decoded = 0;
 _last_requested_element = -1;  // impossible value to mark "unnused" 

}


oncsSub_idtpcfeev3::~oncsSub_idtpcfeev3()
{

  waveform_set::iterator itr;

  for ( itr = waveforms.begin() ; itr  != waveforms.end() ; ++itr)
    {
      delete (*itr);
    }
  waveforms.clear();
  
}

int oncsSub_idtpcfeev3::cacheIterator(const int n)
{
  if ( n < 0) return 0; // not ok
  if ( _last_requested_element == n) return 1; // say "ok"

  unsigned int i = n;
  
  if  ( i >= waveforms.size() )
    {
      _last_requested_element = -1;
      return 0;
    }

  auto it = std::next(waveforms.begin(), i);
  _cached_iter = it;
  return 1;
}



int oncsSub_idtpcfeev3::tpc_decode ()
{

  if (_is_decoded ) return 0;
  _is_decoded = 1;

  unsigned int payload_length = 2 * (getLength() - SEVTHEADERLENGTH - getPadding() );
  
  unsigned int index = 0;
  

  unsigned short *buffer = ( unsigned short *)  &SubeventHdr->data;


  while ( (buffer[index] & 0xFF00 ) == 0xBA00  && (index < payload_length) )
    {
      unsigned int fee_id = buffer[index] & 0xff;
      const unsigned int datalength = 0xf;
//      coutfl << " index = " << index << " fee_id = " << fee_id << " len = " << datalength << endl;
      index++;
      if ( fee_id < MAX_FEECOUNT)
	{
	  for ( unsigned int i = 0;  i < datalength ; i++)
	    {
	      fee_data[fee_id].push_back( buffer[index++]);
	    }
	}
  
    }
//    coutfl << " done with tcp_decode index = " << index << endl;

  // std::vector<unsigned short>::const_iterator fee_data_itr;

//   for ( int ifee = 0 ; ifee < MAX_FEECOUNT ; ifee++)
//     {

//       coutfl << " starting with fee number " << ifee << " size of vector " << fee_data[ifee].size() << endl;
//     }
  
  for ( int ifee = 0 ; ifee < MAX_FEECOUNT ; ifee++)
    {
      
      unsigned int pos;   // I'm too tired to play with iterators; pos is the position in a FEE vector
      
      //for ( fee_data_itr = fee_data[ifee].begin();  fee_data_itr != fee_data[ifee].end();  )
      for ( pos = 0; pos < fee_data[ifee].size() ;   ) 
	{
	  // itr = fee_data_itr;
	  int skip_amount = find_header (pos, fee_data[ifee]);
	  if ( skip_amount < 0) break;
	  //for ( int i = 0; i < skip_amount; i++) ++fee_data_itr;  // skip that many words
	  pos +=  skip_amount;

	  // as we advance pos, let's remember where the start is
	  unsigned int startpos = pos;

	  // first the check if our vector cuts off before the fixed-length header, then we are already done
	  if ( startpos + HEADER_LENGTH >= fee_data[ifee].size() || startpos + fee_data[ifee][startpos] > fee_data[ifee].size())
	    {
	      pos = fee_data[ifee].size() + 1; // make sure we really terminate the loop
	    }
	  else
	    {
	      // capture the header so we can easier get the bit shift stuff
	      uint16_t header[HEADER_LENGTH];
	      for ( int i = 0; i < HEADER_LENGTH; i++ ) header[i] = (fee_data[ifee][pos++]) ;

	      sampa_waveform *sw = new sampa_waveform;
	  
	      sw->fee           = ifee;
	      sw->pkt_length    = header[0];
	      sw->adc_length    = header[5];
	      sw->sampa_address = (header[1] >> 5) & 0xf;
	      sw->sampa_channel = header[1] & 0x1f;
	      sw->channel       = header[1] & 0x1ff;
	      sw->bx_timestamp  = ((header[3] & 0x1ff) << 11)
		| ((header[2] & 0x3ff) << 1)
		| (header[1] >> 9);

	      // now we add the actual waveform
	      uint16_t data_size = header[5] -1 ;


//         coutfl << " Fee: " << ifee << " Sampa " << sw->sampa_address
//           << " sampa channel: " << sw->sampa_channel
//           << " channel: " << sw->channel
//           << "  waveform length: " << data_size  << endl;


	      for (int i = 0 ; i < data_size ; i++)
		{
		  sw->waveform.push_back( fee_data[ifee][pos++]);
		}
	      
	      // we calculate the checksum here because "pos" is at the right place
	      uint16_t crc = crc16(ifee, startpos, header[0]-1);
	      // coutfl << "fee " << setw(3) << sw->fee
	      // 	     << " sampla channel " << setw(3) <<  sw->channel
	      // 	     << " crc and value " << hex << setw(5) << crc << " " << setw(5) << fee_data[ifee][pos] << dec;
	      // if (  crc != fee_data[ifee][pos] ) cout << "  *********";
	      // cout << endl;
	      
	      sw->checksum = crc;
	      sw->valid = ( crc == fee_data[ifee][pos]);
	      
	      waveforms.insert(sw);
	    }
	  
	  //coutfl << "inserting at " << ifee*MAX_CHANNELS + sw->channel << " size is " << waveforms.size() << endl;
	  //waveform_vector[ifee*MAX_CHANNELS + sw->channel].push_back(sw);
	}
    }
  
  return 0;
}


int oncsSub_idtpcfeev3::iValue(const int n, const int sample)
{
  if ( n < 0) return 0;
  
  tpc_decode();

  if ( cacheIterator(n) )
    {
      unsigned int m = sample; 
      if ( m >= (*_cached_iter)->waveform.size() ) return 0;
      return (*_cached_iter)->waveform[m];
    }
  return 0;
}



int oncsSub_idtpcfeev3::iValue(const int fee, const int ch, const int sample)
{
  tpc_decode();
  return 0;
}

int oncsSub_idtpcfeev3::iValue(const int fee, const int ch, const int sample, const char *what)
{
  tpc_decode();
  return 0;
}


int oncsSub_idtpcfeev3::iValue(const int fee, const int ch, const char *what)
{

  tpc_decode();


  return 0;
}
  

int oncsSub_idtpcfeev3::iValue(const int n, const char *what)
{

  tpc_decode();

  if ( strcmp(what,"NR_WF") == 0 )  // the number of datasets
  {
    return waveforms.size();
  }

  else if ( strcmp(what,"MAX_FEECOUNT") == 0 )
  {
    return MAX_FEECOUNT;
  }

  // see how many samples we have
  else if ( strcmp(what,"SAMPLES") == 0 )
    {
      if ( cacheIterator(n) )
	{
	  return (int) (*_cached_iter)->waveform.size();
	}
      return 0;
    }
  
  else if ( strcmp(what,"FEE") == 0 )
    {
      if ( cacheIterator(n) )
	{
	  return (int) (*_cached_iter)->fee;
	}
      return 0;
    }
  
  else if ( strcmp(what,"SAMPAADDRESS") == 0 )
    {
      if ( cacheIterator(n) )
	{
	  return (int) (*_cached_iter)->sampa_address;
	}
      return 0;
    }
  
  else if ( strcmp(what,"SAMPACHANNEL") == 0 )
    {
      if ( cacheIterator(n) )
	{
	  return (int) (*_cached_iter)->sampa_channel;
	}
      return 0;
    }
  
  else if ( strcmp(what,"CHANNEL") == 0 )
    {
      if ( cacheIterator(n) )
	{
	  return (int) (*_cached_iter)->channel;
	}
      return 0;
    }
  
  else if ( strcmp(what,"BCO") == 0 )
    {
      if ( cacheIterator(n) )
	{
	  return (int) (*_cached_iter)->bx_timestamp;
	}
      return 0;
    }

  else if ( strcmp(what,"CHECKSUM") == 0 )
    {
      if ( cacheIterator(n) )
	{
	  return (int) (*_cached_iter)->checksum;
	}
      return 0;
    }

  else if ( strcmp(what,"CHECKSUMERROR") == 0 )
    {
      if ( cacheIterator(n) )
	{
	  if ( (*_cached_iter)->valid ) return 0; 
	  return 1;
	}
      return 0;
    }

  return 0;
  
}
 
//int oncsSub_idtpcfeev3::find_header ( std::vector<unsigned short>::const_iterator &itr,  const std::vector<unsigned short> &orig)
int oncsSub_idtpcfeev3::find_header ( const unsigned int yy,  const std::vector<unsigned short> &orig)
{
  bool found = false;
  unsigned int pos = yy;
  std::vector<unsigned short> header_candidate;

  // we slide over the data and find the header, if any.
  // we calculate and return the amount of words we need to skip to find the vector.
  // if we find it right away, the amount returned is 0;
  // -1 for an error condition or end, like we hit the end without finding another header.
  // the complication here is that we look ahead a few words to recognize it.  
  
  for ( unsigned int i = 0; i < HEADER_LENGTH ; i++)  // we start out with one "header candidate" = 7 entries
    {
      if ( pos >= orig.size())    // if we reached the end, no more header here
	{
	  return -1;
	}

      header_candidate.push_back(orig[pos]);
      pos++;
    }
  
  int skip_amount = 0;
  while ( ! found)
    {
      //      coutfl << " current pos is  " << pos  << "  vector length " << orig.size()  << endl;
	    
      if (header_candidate[4] == MAGIC_KEY_0 && header_candidate[6] == MAGIC_KEY_1 && (header_candidate[0] - header_candidate[5] == HEADER_LENGTH))
	{
	  // found it!
            found = true;
	    //    coutfl << " found header skip value =  " << skip_amount << endl;
            break;
        }
      skip_amount++;
      if ( pos >= orig.size())    // if we reached the end, no more header here
	{
	  return -1;
	}

      //   coutfl << " next value " << pos << "  " << hex << orig[pos]  << dec << endl;
      header_candidate.erase(header_candidate.begin());  // delete the vector 1st element
      header_candidate.push_back(orig[pos]);                  // push a new value, rinse and repeat
      pos++;
    }

  return skip_amount;
}
 
void  oncsSub_idtpcfeev3::dump ( OSTREAM& os )
{
  tpc_decode();
  identify(os);

  //  std::vector<unsigned short>::const_iterator fee_data_iter[MAX_FEECOUNT];

  // for ( int i = 0 ; i < iValue(0,"MAX_FEECOUNT"); i++)
  //   {
  //     os << setw(4) << i << " " << fee_data[i].size()  << endl;
  //   }
  // os << endl;

  // os << "        ";

  // for ( int i = 0 ; i < iValue(0,"MAX_FEECOUNT"); i++)
  //   {
  //     fee_data_iter[i] = fee_data[i].begin();
  //     os << setw(8) << i;
  //   }
  // os << endl;
  // os << "-----------------------------------------------" << endl;

  // int still_data = 1;
  // int count = 0;

  // while (still_data)
  //   {
  //     os << setw(5) << count++ << " | ";
  //     still_data = 0;
  //     for ( int i = 0 ; i < iValue(0,"MAX_FEECOUNT"); i++)
  // 	{
  // 	  if (fee_data_iter[i] != fee_data[i].end())
  // 	    {
  // 	      os << setw(5) << hex << *(fee_data_iter[i]);
  // 	      still_data = 1;
  // 	      ++(fee_data_iter[i]);
  // 	    }
  // 	  else
  // 	    {
  // 	      os << setw(5) << " x ";
  // 	    }
  // 	}
  //     os << dec << endl;
  //   }
  
  waveform_set::const_iterator wf_iter;

  for ( int i = 0; i < iValue(0, "NR_WF") ; i++) // go through the datasets
    {
      os << "  FEE   Channel   Sampachannel   Samples     BCO     CRC_ERR" << endl;

      os << setw(5) << iValue(i, "FEE")  << " "
	 << setw(9) << iValue(i, "CHANNEL")  << " "
	 << setw(9) << iValue(i, "SAMPACHANNEL")  << " "
	 << setw(12) << iValue(i, "SAMPLES") << " "
	 <<  "     0x" << setw(5) << hex << iValue(i, "BCO") << dec
	//	 << " 0x" << setw(4) << hex << iValue(i, "CHECKSUM") << dec
	 <<  setw(4) << iValue(i, "CHECKSUMERROR")
	 << endl;
      
      for (int j = 0; j  < iValue(i, "SAMPLES") ; j += 10)
	{
	  os << "                                                       ";
	  for (int k = 0; k < 10 ; k++)
	    {
	      os << setw(4) << iValue(i,j+k)  << " ";
	    }
	  os << endl;
	}
      os << endl;
  
    }
}



unsigned short oncsSub_idtpcfeev3::reverseBits(const unsigned short x) const
{
  unsigned short n = x;  
  n = ((n >> 1) & 0x55555555) | ((n << 1) & 0xaaaaaaaa);
  n = ((n >> 2) & 0x33333333) | ((n << 2) & 0xcccccccc);
  n = ((n >> 4) & 0x0f0f0f0f) | ((n << 4) & 0xf0f0f0f0);
  n = ((n >> 8) & 0x00ff00ff) | ((n << 8) & 0xff00ff00);
  //n = (n >> 16) & 0x0000ffff | (n << 16) & 0xffff0000;
  return n;
}



unsigned short oncsSub_idtpcfeev3::crc16(const unsigned int fee, const unsigned int index, const int  l) const 
{

  unsigned short crc = 0xffff;
    
  for ( int i = 0; i < l; i++)
    {
      unsigned short x = fee_data[fee][index+i];
      //      cout << "in crc " << hex << x << dec << endl;
      crc ^= reverseBits( x);
      for (unsigned short k = 0; k < 16; k++)
	{
	  crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1;
	}
      }
  crc = reverseBits(crc);
  return crc;
}

