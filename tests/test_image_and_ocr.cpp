#include "tests/test_framework.hpp"
#include "ticket_ocr/corpus_generator.hpp"
#include "ticket_ocr/image_io.hpp"
#include "ticket_ocr/image_preprocessor.hpp"
#include "ticket_ocr/ocr_engine.hpp"

using namespace ticket_ocr;

TEST_CASE(DarkModeDetectionAndNativeOcrIntegration) {
    const auto spec = CorpusGenerator::build_default_corpus_spec();
    // S02 is Dark Mode WebSaaS with 0x80070005
    const CorpusSample& dark_sample = spec[1];
    REQUIRE_TRUE(dark_sample.dark_mode);

    const ImageBuffer raw_img = CorpusGenerator::render_sample_image(dark_sample);
    REQUIRE_TRUE(ImagePreprocessor::detect_dark_mode(raw_img));

    ImagePreprocessor preproc;
    const PreprocessOutput pre_out = preproc.process(raw_img);
    REQUIRE_TRUE(pre_out.diagnostics.is_dark_mode);
    // Inverted image must now appear bright background (not dark mode)
    REQUIRE_TRUE(!ImagePreprocessor::detect_dark_mode(pre_out.primary_image));

    WindowsNativeOcrEngine ocr;
    const OcrResult ocr_res = ocr.recognize(pre_out);
    REQUIRE_TRUE(!ocr_res.raw_text.empty());
    REQUIRE_TRUE(ocr_res.raw_text.find("0x80070005") != std::string::npos);
    REQUIRE_TRUE(ocr_res.raw_text.find("IdentityProvider") != std::string::npos);
}

TEST_CASE(MobileCameraMoireDetectionAndRestorationIntegration) {
    const auto spec = CorpusGenerator::build_default_corpus_spec();
    const CorpusSample& clean_sample = spec[0]; // Clean desktop PNG
    const CorpusSample& moire_sample = spec[4]; // S05: Dark Mode + Mobile Camera Moire!

    const ImageBuffer clean_img = CorpusGenerator::render_sample_image(clean_sample);
    const ImageBuffer moire_img = CorpusGenerator::render_sample_image(moire_sample);

    const double clean_score = ImagePreprocessor::compute_moire_score(clean_img);
    const double moire_score = ImagePreprocessor::compute_moire_score(moire_img);
    REQUIRE_GE(moire_score, clean_score + 0.12);

    ImagePreprocessor preproc;
    const PreprocessOutput pre_out = preproc.process(moire_img);
    REQUIRE_TRUE(pre_out.diagnostics.moire_detected);
    REQUIRE_TRUE(!pre_out.anti_moire_image.empty());

    WindowsNativeOcrEngine ocr;
    const OcrResult ocr_res = ocr.recognize(pre_out);
    REQUIRE_TRUE(ocr_res.raw_text.find("ERR-NET-5260") != std::string::npos);
    REQUIRE_TRUE(ocr_res.raw_text.find("v6.0.2") != std::string::npos);
}

TEST_CASE(LowResolutionUpscalingAndDiskRoundtripIntegration) {
    const auto spec = CorpusGenerator::build_default_corpus_spec();
    const CorpusSample& lowres_sample = spec[5]; // S06: 860x520 compact low-res
    REQUIRE_LE(lowres_sample.width, 900);

    const ImageBuffer img = CorpusGenerator::render_sample_image(lowres_sample);
    const std::filesystem::path tmp_path = std::filesystem::temp_directory_path() / "ticket_ocr_lowres_test.png";
    REQUIRE_TRUE(ImageIO::save_to_file(img, tmp_path));

    auto loaded = ImageIO::load_from_file(tmp_path);
    REQUIRE_TRUE(loaded.has_value());
    REQUIRE_EQ(loaded->width, lowres_sample.width);

    ImagePreprocessor preproc;
    const PreprocessOutput pre_out = preproc.process(*loaded);
    REQUIRE_TRUE(pre_out.diagnostics.upscaled);
    REQUIRE_GE(pre_out.diagnostics.processed_width, 1180);

    WindowsNativeOcrEngine ocr;
    const OcrResult ocr_res = ocr.recognize(pre_out);
    REQUIRE_TRUE(ocr_res.raw_text.find("ECONNREFUSED") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove(tmp_path, ec);
}

int main() {
    return test_fw::run_all_tests("Preprocesamiento Visual (Modo Oscuro, Anti-Moiré, Escalado) + OCR Nativo");
}
