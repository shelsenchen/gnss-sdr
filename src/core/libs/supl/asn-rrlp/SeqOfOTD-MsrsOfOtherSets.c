/*
 * Generated by asn1c-0.9.22 (http://lionet.info/asn1c)
 * From ASN.1 module "RRLP-Components"
 * 	found in "../rrlp-components.asn"
 */

#include "SeqOfOTD-MsrsOfOtherSets.h"

static asn_per_constraints_t ASN_PER_TYPE_SEQ_OF_OTD_MSRS_OF_OTHER_SETS_CONSTR_1 = {
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	{ APC_CONSTRAINED,	 4,  4,  1,  10 }	/* (SIZE(1..10)) */,
	0, 0	/* No PER value map */
};
static asn_TYPE_member_t asn_MBR_SeqOfOTD_MsrsOfOtherSets_1[] = {
	{ ATF_POINTER, 0, 0,
		-1 /* Ambiguous tag (CHOICE?) */,
		0,
		&asn_DEF_OTD_MsrsOfOtherSets,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		""
		},
};
static ber_tlv_tag_t asn_DEF_SeqOfOTD_MsrsOfOtherSets_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static asn_SET_OF_specifics_t asn_SPC_SeqOfOTD_MsrsOfOtherSets_specs_1 = {
	sizeof(struct SeqOfOTD_MsrsOfOtherSets),
	offsetof(struct SeqOfOTD_MsrsOfOtherSets, _asn_ctx),
	2,	/* XER encoding is XMLValueList */
};
asn_TYPE_descriptor_t asn_DEF_SeqOfOTD_MsrsOfOtherSets = {
	"SeqOfOTD-MsrsOfOtherSets",
	"SeqOfOTD-MsrsOfOtherSets",
	SEQUENCE_OF_free,
	SEQUENCE_OF_print,
	SEQUENCE_OF_constraint,
	SEQUENCE_OF_decode_ber,
	SEQUENCE_OF_encode_der,
	SEQUENCE_OF_decode_xer,
	SEQUENCE_OF_encode_xer,
	SEQUENCE_OF_decode_uper,
	SEQUENCE_OF_encode_uper,
	0,	/* Use generic outmost tag fetcher */
	asn_DEF_SeqOfOTD_MsrsOfOtherSets_tags_1,
	sizeof(asn_DEF_SeqOfOTD_MsrsOfOtherSets_tags_1)
		/sizeof(asn_DEF_SeqOfOTD_MsrsOfOtherSets_tags_1[0]), /* 1 */
	asn_DEF_SeqOfOTD_MsrsOfOtherSets_tags_1,	/* Same as above */
	sizeof(asn_DEF_SeqOfOTD_MsrsOfOtherSets_tags_1)
		/sizeof(asn_DEF_SeqOfOTD_MsrsOfOtherSets_tags_1[0]), /* 1 */
	&ASN_PER_TYPE_SEQ_OF_OTD_MSRS_OF_OTHER_SETS_CONSTR_1,
	asn_MBR_SeqOfOTD_MsrsOfOtherSets_1,
	1,	/* Single element */
	&asn_SPC_SeqOfOTD_MsrsOfOtherSets_specs_1	/* Additional specs */
};

