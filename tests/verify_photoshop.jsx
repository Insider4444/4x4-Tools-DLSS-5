// Opt-in application test. Run in an empty Photoshop session using File > Scripts > Browse.
// Optional automation inputs: $.global.FOURX4_TEST_INPUT and FOURX4_TEST_OUTPUT.
(function () {
    if (app.documents.length) throw new Error("Close documents before running this isolated test.");
    var input = $.global.FOURX4_TEST_INPUT ? new File($.global.FOURX4_TEST_INPUT) : File.openDialog("Choose an RGB test image");
    if (!input) return;
    var parent = $.global.FOURX4_TEST_OUTPUT ? new Folder($.global.FOURX4_TEST_OUTPUT) : Folder.selectDialog("Choose a private test-output folder");
    if (!parent) return;
    var output = new Folder(parent.fsName + "/4x4Tools-Photoshop-" + new Date().getTime());
    if (!output.create()) throw new Error("Could not create the test-output folder.");
    var doc = null, previousDialogs = app.displayDialogs, results = [];
    var depths = [BitsPerChannelType.EIGHT, BitsPerChannelType.SIXTEEN, BitsPerChannelType.THIRTYTWO];
    app.displayDialogs = DialogModes.NO;
    try {
        for (var i = 0; i < depths.length; ++i) {
            doc = app.open(input);
            if (doc.mode !== DocumentMode.RGB) throw new Error("Choose an RGB fixture.");
            doc.resizeImage(UnitValue(1089, "px"), UnitValue(613, "px"), 72, ResampleMethod.BICUBIC);
            doc.bitsPerChannel = depths[i];
            var descriptor = new ActionDescriptor();
            descriptor.putDouble(charIDToTypeID("x008"), 2);
            descriptor.putDouble(charIDToTypeID("x099"), 512);
            // The scope UUID is the callable filter event. This also verifies PiPL enablement.
            executeAction(stringIDToTypeID("ca8c8e71-f8ea-433b-a977-3857309e371f"), descriptor, DialogModes.NO);
            if (doc.width.as("px") !== 1089 || doc.height.as("px") !== 613 || doc.bitsPerChannel !== depths[i])
                throw new Error("Processing changed the document dimensions or depth.");
            var options, name;
            if (i === 2) {
                options = new TiffSaveOptions(); options.imageCompression = TIFFEncoding.NONE; options.layers = false;
                name = "rgb32.tif";
            } else { options = new PNGSaveOptions(); name = i === 0 ? "rgb8.png" : "rgb16.png"; }
            doc.saveAs(new File(output.fsName + "/" + name), options, true, Extension.LOWERCASE);
            doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
            results.push("PASS Photoshop " + app.version + ": " + name + ", 1089 x 613, tile core 512");
        }
    } catch (error) { results.push("FAIL " + error + " at line " + error.line); }
    finally { if (doc) doc.close(SaveOptions.DONOTSAVECHANGES); app.displayDialogs = previousDialogs; }
    var log = new File(output.fsName + "/result.txt");
    if (!log.open("w")) throw new Error("Could not write the test report.");
    log.write(results.join("\n")); log.close();
    return results.join("\n") + "\nOutput: " + output.fsName;
}());
