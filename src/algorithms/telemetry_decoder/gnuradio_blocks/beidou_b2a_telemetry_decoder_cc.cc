/*!
 * \file beidou_b2a_telemetry_decoder_cc.cc
 * \brief Implementation of an adapter of a BEIDOU B2a CNAV2 data decoder block
 * to a TelemetryDecoderInterface
 * \note Code added as part of GSoC 2018 program
 * \author Dong Kyeong Lee, 2018. dole7890(at)colorado.edu
 *
 * -------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2015  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *          Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * GNSS-SDR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNSS-SDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNSS-SDR. If not, see <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------
 */


#include "beidou_b2a_telemetry_decoder_cc.h"
#include "control_message_factory.h"
#include "convolutional.h"
#include "display.h"
#include "gnss_synchro.h"
#include <boost/lexical_cast.hpp>
#include <gnuradio/io_signature.h>
#include <glog/logging.h>
#include <volk_gnsssdr/volk_gnsssdr.h>
#include <iostream>

#define CRC_ERROR_LIMIT 8

using google::LogMessage;


beidou_b2a_telemetry_decoder_cc_sptr
beidou_b2a_make_telemetry_decoder_cc(const Gnss_Satellite &satellite, bool dump)
{
    return beidou_b2a_telemetry_decoder_cc_sptr(new beidou_b2a_telemetry_decoder_cc(satellite, dump));
}


beidou_b2a_telemetry_decoder_cc::beidou_b2a_telemetry_decoder_cc(
    const Gnss_Satellite &satellite,
    bool dump) : gr::block("beidou_b2a_telemetry_decoder_cc", gr::io_signature::make(1, 1, sizeof(Gnss_Synchro)),
                     gr::io_signature::make(1, 1, sizeof(Gnss_Synchro)))
{
    // Ephemeris data port out
    this->message_port_register_out(pmt::mp("telemetry"));
    // initialize internal vars
    d_dump = dump;
    d_satellite = Gnss_Satellite(satellite.get_system(), satellite.get_PRN());
    LOG(INFO) << "Initializing BEIDOU B2a TELEMETRY DECODING";
    // Define the number of sampes per symbol. Notice that BEIDOU has 2 rates, !!!Change
    //one for the navigation data and the other for the preamble information
    d_samples_per_symbol = (BEIDOU_B2a_CODE_RATE_HZ / BEIDOU_B2a_CODE_LENGTH_CHIPS) / BEIDOU_B2a_SYMBOL_RATE_SPS;

    // Set the preamble information
    uint16_t preambles_bits[BEIDOU_CNAV2_PREAMBLE_LENGTH_BITS] = BEIDOU_CNAV2_PREAMBLE;
    // Since preamble rate is different than navigation data rate we use a constant
    d_symbols_per_preamble = BEIDOU_CNAV2_PREAMBLE_LENGTH_SYMBOLS;

    memcpy(static_cast<uint16_t *>(this->d_preambles_bits), static_cast<uint16_t *>(preambles_bits), BEIDOU_CNAV2_PREAMBLE_LENGTH_BITS * sizeof(uint16_t));

    // preamble bits to sampled symbols
    d_preambles_symbols = static_cast<int32_t *>(malloc(sizeof(int32_t) * d_symbols_per_preamble));
    int32_t n = 0;
    for (int32_t i = 0; i < BEIDOU_CNAV2_PREAMBLE_LENGTH_BITS; i++)
        {
            for (uint32_t j = 0; j < BEIDOU_CNAV2_TELEMETRY_SYMBOLS_PER_PREAMBLE_BIT; j++)
                {
                    if (d_preambles_bits[i] == 1)
                        {
                            d_preambles_symbols[n] = 1;
                        }
                    else
                        {
                            d_preambles_symbols[n] = -1;
                        }
                    n++;
                }
        }
    d_sample_counter = 0;
    d_stat = 0;
    d_preamble_index = 0;

    d_flag_frame_sync = false;

    d_flag_parity = false;
    d_TOW_at_current_symbol = 0;
    Flag_valid_word = false;
    delta_t = 0;
    d_CRC_error_counter = 0;
    d_flag_preamble = false;
    d_channel = 0;
    flag_TOW_set = false;
    d_preamble_time_samples = 0;
}


beidou_b2a_telemetry_decoder_cc::~beidou_b2a_telemetry_decoder_cc()
{
    delete d_preambles_symbols;
    if (d_dump_file.is_open() == true)
        {
            try
                {
                    d_dump_file.close();
                }
            catch (const std::exception &ex)
                {
                    LOG(WARNING) << "Exception in destructor closing the dump file " << ex.what();
                }
        }
}


void beidou_b2a_telemetry_decoder_cc::decode_string(double *frame_symbols, int frame_length)
{
    // 1. Transform from symbols to bits
    std::string data_bits;

    // we want data_bits = frame_symbols[24:311]
    assert(frame_length >= 24+288); // or whatever error checking mechanism exists in this cb
    for (unsigned ii = 24; ii < (24+288); ++ii) {
    	char bit_value;

    	if (frame_symbols[ii] > 0)
    		bit_value = '1';
    	else
    		bit_value = '0';

    	data_bits.push_back(bit_value);

    	// "ternary" operator
    	//data_bits.push_back( (frame_symbols[ii] > 0) ? ('1') : ('0') );
    }


    // 2. Call the BEIDOU CNAV2 string decoder
    d_nav.string_decoder(data_bits);

    // 3. Check operation executed correctly
    if (d_nav.flag_CRC_test == true)
        {
            LOG(INFO) << "BEIDOU CNAV2 CRC correct in channel " << d_channel << " from satellite " << d_satellite;
        }
    else
        {
            LOG(INFO) << "BEIDOU CNAV2 CRC error in channel " << d_channel << " from satellite " << d_satellite;
        }
    // 4. Push the new navigation data to the queues
    if (d_nav.have_new_ephemeris() == true)
        {
            // get object for this SV (mandatory)
            std::shared_ptr<Beidou_Cnav2_Ephemeris> tmp_obj = std::make_shared<Beidou_Cnav2_Ephemeris>(d_nav.get_ephemeris());
            this->message_port_pub(pmt::mp("telemetry"), pmt::make_any(tmp_obj));
            LOG(INFO) << "BEIDOU CNAV2 Ephemeris have been received in channel" << d_channel << " from satellite " << d_satellite;
            std::cout << "New BEIDOU B2a CNAV2 message received in channel " << d_channel << ": ephemeris from satellite " << d_satellite << std::endl;
        }
    if (d_nav.have_new_utc_model() == true)
        {
            // get object for this SV (mandatory)
            std::shared_ptr<Beidou_Cnav2_Utc_Model> tmp_obj = std::make_shared<Beidou_Cnav2_Utc_Model>(d_nav.get_utc_model());
            this->message_port_pub(pmt::mp("telemetry"), pmt::make_any(tmp_obj));
            LOG(INFO) << "BEIDOU CNAV2 UTC Model have been received in channel" << d_channel << " from satellite " << d_satellite;
            std::cout << "New BEIDOU B2a CNAV2 message received in channel " << d_channel << ": UTC model parameters from satellite " << d_satellite << std::endl;
        }
    if (d_nav.have_new_almanac() == true)
        {
            unsigned int slot_nbr = d_nav.i_alm_satellite_slot_number;
            std::shared_ptr<Beidou_Cnav2_Almanac> tmp_obj = std::make_shared<Beidou_Cnav2_Almanac>(d_nav.get_almanac(slot_nbr));
            this->message_port_pub(pmt::mp("telemetry"), pmt::make_any(tmp_obj));
            LOG(INFO) << "BEIDOU CNAV2 Almanac have been received in channel" << d_channel << " in slot number " << slot_nbr;
            std::cout << "New BEIDOU B2a CNAV2 almanac received in channel " << d_channel << " from satellite " << d_satellite << std::endl;
        }
    // 5. Update satellite information on system
    if (d_nav.flag_update_slot_number == true)
        {
            LOG(INFO) << "BEIDOU CNAV2 Slot Number Identified in channel " << d_channel;
            d_satellite.update_PRN(d_nav.cnav2_ephemeris.SatType);
            d_satellite.what_block(d_satellite.get_system(), d_nav.cnav2_ephemeris.SatType);
            d_nav.flag_update_slot_number = false;
        }
}


void beidou_b2a_telemetry_decoder_cc::set_satellite(const Gnss_Satellite &satellite)
{
    d_satellite = Gnss_Satellite(satellite.get_system(), satellite.get_PRN());
    DLOG(INFO) << "Setting decoder Finite State Machine to satellite " << d_satellite;
    DLOG(INFO) << "Navigation Satellite set to " << d_satellite;
}


void beidou_b2a_telemetry_decoder_cc::set_channel(int channel)
{
    d_channel = channel;
    LOG(INFO) << "Navigation channel set to " << channel;
    // ############# ENABLE DATA FILE LOG #################
    if (d_dump == true)
        {
            if (d_dump_file.is_open() == false)
                {
                    try
                        {
                            d_dump_filename = "telemetry";
                            d_dump_filename.append(boost::lexical_cast<std::string>(d_channel));
                            d_dump_filename.append(".dat");
                            d_dump_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                            d_dump_file.open(d_dump_filename.c_str(), std::ios::out | std::ios::binary);
                            LOG(INFO) << "Telemetry decoder dump enabled on channel " << d_channel << " Log file: " << d_dump_filename.c_str();
                        }
                    catch (const std::ifstream::failure &e)
                        {
                            LOG(WARNING) << "channel " << d_channel << ": exception opening Beidou TLM dump file. " << e.what();
                        }
                }
        }
}


int beidou_b2a_telemetry_decoder_cc::general_work(int noutput_items __attribute__((unused)), gr_vector_int &ninput_items __attribute__((unused)),
    gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	int32_t corr_value = 0;
	int32_t preamble_diff = 0;

    Gnss_Synchro **out = reinterpret_cast<Gnss_Synchro **>(&output_items[0]);            // Get the output buffer pointer
    const Gnss_Synchro **in = reinterpret_cast<const Gnss_Synchro **>(&input_items[0]);  // Get the input buffer pointer

    Gnss_Synchro current_symbol;  //structure to save the synchronization information and send the output object to the next block
    //1. Copy the current tracking output
    current_symbol = in[0][0];
    d_symbol_history.push_back(current_symbol);  //add new symbol to the symbol queue
    d_sample_counter++;                          //count for the processed samples
    consume_each(1);

    d_flag_preamble = false;
    unsigned int required_symbols = BEIDOU_CNAV2_STRING_SYMBOLS;

    if (d_symbol_history.size() > required_symbols)
        {
            //******* preamble correlation ********
            for (int i = 0; i < d_symbols_per_preamble; i++)
                {
                    if (d_symbol_history.at(i).Prompt_I < 0)  // symbols clipping
                        {
                            corr_value -= d_preambles_symbols[i];
                        }
                    else
                        {
                            corr_value += d_preambles_symbols[i];
                        }
                }
        }

    //******* frame sync ******************
    if (d_stat == 0)  //no preamble information
        {
            if (abs(corr_value) >= d_symbols_per_preamble)
                {
                    // Record the preamble sample stamp
                    d_preamble_index = d_sample_counter;
                    LOG(INFO) << "Preamble detection for BEIDOU B2a SAT " << this->d_satellite;
                    // Enter into frame pre-detection status
                    d_stat = 1;
                    d_preamble_time_samples = d_symbol_history.at(0).Tracking_sample_counter;  // record the preamble sample stamp
                }
        }
    else if (d_stat == 1)  // possible preamble lock
        {
            if (abs(corr_value) >= d_symbols_per_preamble)
                {
                    //check preamble separation
                    preamble_diff = d_sample_counter - d_preamble_index;
                    // Record the PRN start sample index associated to the preamble
                    d_preamble_time_samples = d_symbol_history.at(0).Tracking_sample_counter;
                    if (abs(preamble_diff - BEIDOU_CNAV2_PREAMBLE_PERIOD_SYMBOLS) == 0)
                        {
                            //try to decode frame
                            LOG(INFO) << "Starting string decoder for BEIDOU B2a SAT " << this->d_satellite;
                            d_preamble_index = d_sample_counter;  //record the preamble sample stamp
                            d_stat = 2;
                        }
                    else
                        {
                            if (preamble_diff > BEIDOU_CNAV2_PREAMBLE_PERIOD_SYMBOLS)
                                {
                                    d_stat = 0;  // start again
                                }
                            DLOG(INFO) << "Failed string decoder for BEIDOU B2a SAT " << this->d_satellite;
                        }
                }
        }
    else if (d_stat == 2)
        {
            // FIXME: The preamble index marks the first symbol of the string count. Here I just wait for another full string to be received before processing
            if (d_sample_counter == d_preamble_index + BEIDOU_CNAV2_STRING_SYMBOLS)
                {
                    // NEW BEIDOU string received
                    // 0. fetch the symbols into an array
                    int string_length = BEIDOU_CNAV2_STRING_SYMBOLS - d_symbols_per_preamble;
                    double string_symbols[BEIDOU_CNAV2_DATA_SYMBOLS] = {0};

                    //******* SYMBOL TO BIT *******
                    for (int i = 0; i < string_length; i++)
                        {
                            if (corr_value > 0)
                                {
                                    string_symbols[i] = d_symbol_history.at(i + d_symbols_per_preamble).Prompt_I;  // because last symbol of the preamble is just received now!
                                }
                            else
                                {
                                    string_symbols[i] = -d_symbol_history.at(i + d_symbols_per_preamble).Prompt_I;  // because last symbol of the preamble is just received now!
                                }
                        }

                    //call the decoder
                    decode_string(string_symbols, string_length);
                    if (d_nav.flag_CRC_test == true)
                        {
                            d_CRC_error_counter = 0;
                            d_flag_preamble = true;               //valid preamble indicator (initialized to false every work())
                            d_preamble_index = d_sample_counter;  //record the preamble sample stamp (t_P)
                            if (!d_flag_frame_sync)
                                {
                                    d_flag_frame_sync = true;
                                    DLOG(INFO) << " Frame sync SAT " << this->d_satellite << " with preamble start at "
                                               << d_symbol_history.at(0).Tracking_sample_counter << " [samples]";
                                }
                        }
                    else
                        {
                            d_CRC_error_counter++;
                            d_preamble_index = d_sample_counter;  //record the preamble sample stamp
                            if (d_CRC_error_counter > CRC_ERROR_LIMIT)
                                {
                                    LOG(INFO) << "Lost of frame sync SAT " << this->d_satellite;
                                    d_flag_frame_sync = false;
                                    d_stat = 0;
                                }
                        }
                }
        }

    // UPDATE GNSS SYNCHRO DATA
    //2. Add the telemetry decoder information
    if (this->d_flag_preamble == true and d_nav.flag_TOW_new == true)
        //update TOW at the preamble instant
        {
            d_TOW_at_current_symbol = floor((d_nav.cnav2_ephemeris.SOW - BEIDOU_CNAV2_PREAMBLE_DURATION_S) * 1000) / 1000;
            d_nav.flag_TOW_new = false;
        }
    else  //if there is not a new preamble, we define the TOW of the current symbol
        {
            d_TOW_at_current_symbol = d_TOW_at_current_symbol + BEIDOU_B2a_CODE_PERIOD;
        }

    //if (d_flag_frame_sync == true and d_nav.flag_TOW_set==true and d_nav.flag_CRC_test == true)

    // if(d_nav.flag_GGTO_1 == true  and  d_nav.flag_GGTO_2 == true and  d_nav.flag_GGTO_3 == true and  d_nav.flag_GGTO_4 == true) //all GGTO parameters arrived
    //     {
    //         delta_t = d_nav.A_0G_10 + d_nav.A_1G_10 * (d_TOW_at_current_symbol - d_nav.t_0G_10 + 604800.0 * (fmod((d_nav.WN_0 - d_nav.WN_0G_10), 64)));
    //     }

    if (d_flag_frame_sync == true and d_nav.flag_TOW_set == true)
        {
            current_symbol.Flag_valid_word = true;
        }
    else
        {
            current_symbol.Flag_valid_word = false;
        }

    current_symbol.PRN = this->d_satellite.get_PRN();
    current_symbol.TOW_at_current_symbol_ms = d_TOW_at_current_symbol;
    current_symbol.TOW_at_current_symbol_ms -= delta_t;  // Beidou to GPS TOW !!! check

    if (d_dump == true)
        {
            // MULTIPLEXED FILE RECORDING - Record results to file
            try
                {
                    double tmp_double;
                    unsigned long int tmp_ulong_int;
                    tmp_double = d_TOW_at_current_symbol;
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_double), sizeof(double));
                    tmp_ulong_int = current_symbol.Tracking_sample_counter;
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_ulong_int), sizeof(unsigned long int));
                    tmp_double = 0;
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_double), sizeof(double));
                }
            catch (const std::ifstream::failure &e)
                {
                    LOG(WARNING) << "Exception writing observables dump file " << e.what();
                }
        }

    // remove used symbols from history
    if (d_symbol_history.size() > required_symbols)
        {
            d_symbol_history.pop_front();
        }
    //3. Make the output (copy the object contents to the GNURadio reserved memory)
    *out[0] = current_symbol;

    return 1;
}
