#include "tcp_framing.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

static int s_passed = 0;
static int s_failed = 0;

static void check(bool condition, const char* name)
{
	if (condition)
	{
		++s_passed;
	}
	else
	{
		++s_failed;
		std::printf("  FAIL: %s\n", name);
	}
}

/* ── Receive: DMI extraction ── */

static void test_dmi_empty_buffer()
{
	std::vector<unsigned char> buf;
	unsigned char out[256];
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_dmi_frame(buf, out, sizeof(out), &received);
	check(!ok, "dmi: empty buffer returns false");
}

static void test_dmi_complete_min_frame()
{
	std::vector<unsigned char> buf = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x00 };
	unsigned char out[256] = {};
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_dmi_frame(buf, out, sizeof(out), &received);
	check(ok, "dmi: complete 6-byte frame extracts");
	check(received == 6, "dmi: received_size is 6");
	check(out[0] == 0x55 && out[1] == 0xAA, "dmi: magic bytes preserved");
	check(buf.empty(), "dmi: buffer consumed after extraction");
}

static void test_dmi_frame_with_payload()
{
	std::vector<unsigned char> buf = { 0x55, 0xAA, 0x00, 0x09, 0x00, 0x00, 'A', 'B', 'C' };
	unsigned char out[256] = {};
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_dmi_frame(buf, out, sizeof(out), &received);
	check(ok, "dmi: frame with 3-byte payload extracts");
	check(received == 9, "dmi: payload frame received_size is 9");
	check(out[6] == 'A' && out[7] == 'B' && out[8] == 'C', "dmi: payload bytes preserved");
}

static void test_dmi_partial_chunks()
{
	std::vector<unsigned char> buf = { 0x55, 0xAA, 0x00 };
	unsigned char out[256] = {};
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_dmi_frame(buf, out, sizeof(out), &received);
	check(!ok, "dmi: partial (3 bytes) returns false");
	/* Append the rest */
	buf.insert(buf.end(), { 0x06, 0x00, 0x00 });
	ok = tcp_framing_try_extract_dmi_frame(buf, out, sizeof(out), &received);
	check(ok, "dmi: after appending rest, extraction succeeds");
}

static void test_dmi_stray_byte_before_magic()
{
	std::vector<unsigned char> buf = { 0x00, 0x55, 0xAA, 0x00, 0x06, 0x00, 0x00 };
	unsigned char out[256] = {};
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_dmi_frame(buf, out, sizeof(out), &received);
	check(ok, "dmi: stray byte before magic discarded and frame extracted");
	check(received == 6, "dmi: correct frame after discarding stray");
}

static void test_dmi_length_too_small()
{
	std::vector<unsigned char> buf = { 0x55, 0xAA, 0x00, 0x03, 0x00, 0x00 };
	unsigned char out[256] = {};
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_dmi_frame(buf, out, sizeof(out), &received);
	check(!ok && buf.empty(), "dmi: sub-minimum length byte discarded, buffer empty");
}

static void test_dmi_frame_larger_than_capacity()
{
	std::vector<unsigned char> buf = { 0x55, 0xAA, 0x00, 0x10, 0x00, 0x00, 0,0,0,0,0,0,0,0,0,0 };
	unsigned char out[8] = {};
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_dmi_frame(buf, out, sizeof(out), &received);
	check(!ok, "dmi: frame larger than capacity returns false");
	check(received == 16, "dmi: *received_size reports actual frame size");
}

/* ── Receive: two-byte-length extraction ── */

static void test_len_big_complete()
{
	std::vector<unsigned char> buf = { 0x00, 0x05, 'H', 'E', 'L', 'L', 'O' };
	unsigned char out[256] = {};
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_two_byte_len_frame(buf, out, sizeof(out), &received, true);
	check(ok, "2-byte-len-big: extracts 5-byte payload");
	check(received == 5, "2-byte-len-big: received_size is 5");
	check(0 == std::memcmp(out, "HELLO", 5), "2-byte-len-big: payload is HELLO");
}

static void test_len_little_complete()
{
	std::vector<unsigned char> buf = { 0x05, 0x00, 'H', 'E', 'L', 'L', 'O' };
	unsigned char out[256] = {};
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_two_byte_len_frame(buf, out, sizeof(out), &received, false);
	check(ok, "2-byte-len-little: extracts 5-byte payload");
	check(received == 5, "2-byte-len-little: received_size is 5");
}

static void test_len_zero_discarded()
{
	std::vector<unsigned char> buf = { 0x00, 0x00 };
	unsigned char out[256] = {};
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_two_byte_len_frame(buf, out, sizeof(out), &received, false);
	check(!ok && buf.empty(), "2-byte-len: zero-length header discarded");
}

static void test_len_partial_header()
{
	std::vector<unsigned char> buf = { 0x00 };
	unsigned char out[256] = {};
	std::size_t received = 0;
	bool ok = tcp_framing_try_extract_two_byte_len_frame(buf, out, sizeof(out), &received, false);
	check(!ok, "2-byte-len: 1-byte header returns false");
	check(buf.size() == 1, "2-byte-len: partial header preserved in buffer");
}

/* ── Send: DMI framing ── */

static void test_send_dmi_3byte_payload()
{
	const unsigned char payload[] = { 'X', 'Y', 'Z' };
	unsigned char out[256] = {};
	std::size_t sz = tcp_framing_build_dmi_send_frame(payload, 3, out, sizeof(out));
	check(sz == 9, "send-dmi: 3-byte payload produces 9-byte frame");
	check(out[0] == 0x55 && out[1] == 0xAA, "send-dmi: magic 0x55 0xAA");
	check(out[2] == 0x00, "send-dmi: byte[2] reserved=0");
	check(out[3] == 0x09, "send-dmi: length byte = 9");
	check(out[4] == 0x00 && out[5] == 0x00, "send-dmi: bytes[4-5] reserved=0");
	check(out[6] == 'X' && out[7] == 'Y' && out[8] == 'Z', "send-dmi: payload X Y Z");
}

static void test_send_dmi_max_payload()
{
	unsigned char payload[249];
	std::memset(payload, 0x42, sizeof(payload));
	unsigned char out[256] = {};
	std::size_t sz = tcp_framing_build_dmi_send_frame(payload, 249, out, sizeof(out));
	check(sz == 255, "send-dmi: 249-byte payload produces 255-byte frame (max)");
	check(out[3] == 255, "send-dmi: length byte = 255");
}

static void test_send_dmi_overflow()
{
	unsigned char payload[250];
	unsigned char out[512] = {};
	std::size_t sz = tcp_framing_build_dmi_send_frame(payload, 250, out, sizeof(out));
	check(sz == 0, "send-dmi: 250-byte payload returns 0 (overflow)");
}

static void test_send_dmi_zero_payload()
{
	unsigned char out[256] = {};
	std::size_t sz = tcp_framing_build_dmi_send_frame(nullptr, 0, out, sizeof(out));
	check(sz == 6, "send-dmi: 0-byte payload produces 6-byte header-only frame");
	check(out[3] == 6, "send-dmi: empty frame length = 6");
}

/* ── Send: two-byte-length framing ── */

static void test_send_len_big()
{
	const unsigned char payload[] = { 'P', 'I', 'N', 'G', '!' };
	unsigned char out[256] = {};
	std::size_t sz = tcp_framing_build_two_byte_len_send_frame(payload, 5, out, sizeof(out), true);
	check(sz == 7, "send-len-big: 5-byte payload produces 7-byte frame");
	check(out[0] == 0x00 && out[1] == 0x05, "send-len-big: header [0x00, 0x05]");
	check(out[2] == 'P' && out[6] == '!', "send-len-big: payload preserved");
}

static void test_send_len_little()
{
	const unsigned char payload[] = { '!', 'G', 'N', 'I', 'P' };
	unsigned char out[256] = {};
	std::size_t sz = tcp_framing_build_two_byte_len_send_frame(payload, 5, out, sizeof(out), false);
	check(sz == 7, "send-len-little: 5-byte payload produces 7-byte frame");
	check(out[0] == 0x05 && out[1] == 0x00, "send-len-little: header [0x05, 0x00]");
}

/* ── Round-trip: build then extract ── */

static void test_roundtrip_dmi()
{
	const unsigned char payload[] = { 0x01, 0x02, 0x03, 0x04 };
	unsigned char framed[256] = {};
	std::size_t framed_sz = tcp_framing_build_dmi_send_frame(payload, 4, framed, sizeof(framed));
	check(framed_sz == 10, "roundtrip-dmi: framed size is 10");

	std::vector<unsigned char> buf(framed, framed + framed_sz);
	unsigned char extracted[256] = {};
	std::size_t extracted_sz = 0;
	bool ok = tcp_framing_try_extract_dmi_frame(buf, extracted, sizeof(extracted), &extracted_sz);
	check(ok, "roundtrip-dmi: extraction succeeded");
	check(extracted_sz == 10, "roundtrip-dmi: extracted frame size matches");
	check(0 == std::memcmp(extracted + 6, payload, 4), "roundtrip-dmi: payload round-trips");
}

static void test_roundtrip_two_byte_len_big()
{
	const unsigned char payload[] = "round-trip-test";
	const std::size_t plen = std::strlen(reinterpret_cast<const char*>(payload));
	unsigned char framed[256] = {};
	std::size_t framed_sz = tcp_framing_build_two_byte_len_send_frame(payload, plen, framed, sizeof(framed), true);

	std::vector<unsigned char> buf(framed, framed + framed_sz);
	unsigned char extracted[256] = {};
	std::size_t extracted_sz = 0;
	bool ok = tcp_framing_try_extract_two_byte_len_frame(buf, extracted, sizeof(extracted), &extracted_sz, true);
	check(ok, "roundtrip-len-big: extraction succeeded");
	check(extracted_sz == plen, "roundtrip-len-big: payload size matches");
	check(0 == std::memcmp(extracted, payload, plen), "roundtrip-len-big: payload round-trips");
}

static void test_roundtrip_two_byte_len_little()
{
	const unsigned char payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
	unsigned char framed[256] = {};
	std::size_t framed_sz = tcp_framing_build_two_byte_len_send_frame(payload, 4, framed, sizeof(framed), false);

	std::vector<unsigned char> buf(framed, framed + framed_sz);
	unsigned char extracted[256] = {};
	std::size_t extracted_sz = 0;
	bool ok = tcp_framing_try_extract_two_byte_len_frame(buf, extracted, sizeof(extracted), &extracted_sz, false);
	check(ok, "roundtrip-len-little: extraction succeeded");
	check(extracted_sz == 4, "roundtrip-len-little: payload size matches");
	check(extracted[0] == 0xDE && extracted[3] == 0xEF, "roundtrip-len-little: payload round-trips");
}

/* ── has_dmi_magic ── */

static void test_has_dmi_magic()
{
	std::vector<unsigned char> buf = { 0x00, 0x55, 0xAA, 0x06 };
	check(!tcp_framing_has_dmi_magic(buf, 0), "magic: index 0 = false (0x00,0x55)");
	check(tcp_framing_has_dmi_magic(buf, 1), "magic: index 1 = true (0x55,0xAA)");
	check(!tcp_framing_has_dmi_magic(buf, 2), "magic: index 2 = false (0xAA,0x06)");
	check(!tcp_framing_has_dmi_magic(buf, 3), "magic: index 3 = false (not enough bytes)");
}

/* ── Receive: NDJSON ── */

static std::vector<unsigned char> to_buf(const char* text)
{
	const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
	return std::vector<unsigned char>(p, p + std::strlen(text));
}

static void test_ndjson_no_newline_incomplete()
{
	std::vector<unsigned char> buf = to_buf("{\"a\":1}");
	unsigned char out[64] = {};
	std::size_t sz = 0;
	check(!tcp_framing_try_extract_ndjson_line(buf, out, sizeof(out), &sz),
		"ndjson: no terminator returns false");
	check(buf.size() == 7, "ndjson: buffer untouched on incomplete line");
}

static void test_ndjson_single_line()
{
	std::vector<unsigned char> buf = to_buf("{\"a\":1}\n");
	unsigned char out[64] = {};
	std::size_t sz = 0;
	check(tcp_framing_try_extract_ndjson_line(buf, out, sizeof(out), &sz),
		"ndjson: complete line extracted");
	check(sz == 7 && 0 == std::memcmp(out, "{\"a\":1}", 7), "ndjson: line delivered without newline");
	check(buf.empty(), "ndjson: buffer drained after extraction");
}

static void test_ndjson_two_lines_one_chunk()
{
	std::vector<unsigned char> buf = to_buf("{\"n\":1}\n{\"n\":2}\n");
	unsigned char out[64] = {};
	std::size_t sz = 0;
	check(tcp_framing_try_extract_ndjson_line(buf, out, sizeof(out), &sz)
		&& sz == 7 && 0 == std::memcmp(out, "{\"n\":1}", 7),
		"ndjson: first line extracted");
	check(tcp_framing_try_extract_ndjson_line(buf, out, sizeof(out), &sz)
		&& sz == 7 && 0 == std::memcmp(out, "{\"n\":2}", 7),
		"ndjson: second line extracted from buffer");
	check(!tcp_framing_try_extract_ndjson_line(buf, out, sizeof(out), &sz),
		"ndjson: empty buffer after both lines");
}

static void test_ndjson_crlf_stripped()
{
	std::vector<unsigned char> buf = to_buf("{\"a\":1}\r\n");
	unsigned char out[64] = {};
	std::size_t sz = 0;
	check(tcp_framing_try_extract_ndjson_line(buf, out, sizeof(out), &sz),
		"ndjson: CRLF line extracted");
	check(sz == 7 && 0 == std::memcmp(out, "{\"a\":1}", 7), "ndjson: CR stripped from line end");
}

static void test_ndjson_skips_empty_lines()
{
	std::vector<unsigned char> buf = to_buf("\n\n{\"a\":1}\n");
	unsigned char out[64] = {};
	std::size_t sz = 0;
	check(tcp_framing_try_extract_ndjson_line(buf, out, sizeof(out), &sz),
		"ndjson: empty lines skipped");
	check(sz == 7 && 0 == std::memcmp(out, "{\"a\":1}", 7), "ndjson: first non-empty line returned");
}

static void test_ndjson_oversized_line_dropped()
{
	std::vector<unsigned char> buf = to_buf("0123456789\n{ok}\n");
	unsigned char out[4] = {};
	std::size_t sz = 0;
	check(tcp_framing_try_extract_ndjson_line(buf, out, sizeof(out), &sz),
		"ndjson: oversized line dropped, next line delivered");
	check(sz == 4 && 0 == std::memcmp(out, "{ok}", 4), "ndjson: oversized line content skipped");
}

/* ── Send: NDJSON ── */

static void test_send_ndjson_appends_newline()
{
	const unsigned char payload[] = { '{', '}', };
	unsigned char framed[64] = {};
	std::size_t sz = tcp_framing_build_ndjson_send_frame(payload, 2, framed, sizeof(framed));
	check(sz == 3, "send-ndjson: newline appended");
	check(framed[2] == 0x0A, "send-ndjson: terminated with LF");
}

static void test_send_ndjson_no_double_newline()
{
	const unsigned char payload[] = { 'a', '\n' };
	unsigned char framed[64] = {};
	std::size_t sz = tcp_framing_build_ndjson_send_frame(payload, 2, framed, sizeof(framed));
	check(sz == 2 && framed[1] == '\n', "send-ndjson: already-terminated payload unchanged");
}

static void test_send_ndjson_capacity()
{
	unsigned char payload[8];
	std::memset(payload, 'x', sizeof(payload));
	unsigned char framed[8] = {};
	check(tcp_framing_build_ndjson_send_frame(payload, 8, framed, sizeof(framed)) == 0,
		"send-ndjson: zero return when capacity exceeded");
}

static void test_roundtrip_ndjson()
{
	const char* text = "{\"type\":\"a_train\",\"dummy\":true}";
	const std::size_t len = std::strlen(text);
	unsigned char framed[128] = {};
	std::size_t framed_sz = tcp_framing_build_ndjson_send_frame(text, len, framed, sizeof(framed));
	check(framed_sz == len + 1, "roundtrip-ndjson: framed size = payload + LF");

	std::vector<unsigned char> buf(framed, framed + framed_sz);
	unsigned char extracted[128] = {};
	std::size_t extracted_sz = 0;
	check(tcp_framing_try_extract_ndjson_line(buf, extracted, sizeof(extracted), &extracted_sz),
		"roundtrip-ndjson: extraction succeeded");
	check(extracted_sz == len && 0 == std::memcmp(extracted, text, len),
		"roundtrip-ndjson: payload round-trips");
}

int main()
{
	std::printf("=== tcp_framing unit tests ===\n\n");

	std::printf("--- Receive: DMI ---\n");
	test_dmi_empty_buffer();
	test_dmi_complete_min_frame();
	test_dmi_frame_with_payload();
	test_dmi_partial_chunks();
	test_dmi_stray_byte_before_magic();
	test_dmi_length_too_small();
	test_dmi_frame_larger_than_capacity();

	std::printf("\n--- Receive: two-byte-length ---\n");
	test_len_big_complete();
	test_len_little_complete();
	test_len_zero_discarded();
	test_len_partial_header();

	std::printf("\n--- Send: DMI ---\n");
	test_send_dmi_3byte_payload();
	test_send_dmi_max_payload();
	test_send_dmi_overflow();
	test_send_dmi_zero_payload();

	std::printf("\n--- Send: two-byte-length ---\n");
	test_send_len_big();
	test_send_len_little();

	std::printf("\n--- Round-trip ---\n");
	test_roundtrip_dmi();
	test_roundtrip_two_byte_len_big();
	test_roundtrip_two_byte_len_little();

	std::printf("\n--- Magic detection ---\n");
	test_has_dmi_magic();

	std::printf("\n--- Receive: NDJSON ---\n");
	test_ndjson_no_newline_incomplete();
	test_ndjson_single_line();
	test_ndjson_two_lines_one_chunk();
	test_ndjson_crlf_stripped();
	test_ndjson_skips_empty_lines();
	test_ndjson_oversized_line_dropped();

	std::printf("\n--- Send: NDJSON ---\n");
	test_send_ndjson_appends_newline();
	test_send_ndjson_no_double_newline();
	test_send_ndjson_capacity();

	std::printf("\n--- Round-trip: NDJSON ---\n");
	test_roundtrip_ndjson();

	std::printf("\n=== Results: %d passed, %d failed ===\n", s_passed, s_failed);
	return (s_failed == 0) ? 0 : 1;
}
