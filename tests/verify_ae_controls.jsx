// Isolated application QA. Refuses to touch an existing project.
(function () {
    var root=new File($.fileName).parent.parent;
    var artifacts=new Folder(root.fsName+'/artifacts');artifacts.create();
    var base=new Folder(artifacts.fsName+'/ae-controls-v1.0-'+new Date().getTime());
    if(!base.create())throw new Error('Could not create diagnostic folder.');
    var log=new File(base.fsName+'/result.txt');
    function record(text){if(log.open('a')){log.writeln(text);log.close();}$.writeln(text);}
    function find(group,name){
        for(var i=1;i<=group.numProperties;++i){var p=group.property(i);if(p.name===name)return p;
            if(p.propertyType!==PropertyType.PROPERTY){var found=find(p,name);if(found)return found;}}
        return null;
    }
    function dump(group,indent){for(var i=1;i<=group.numProperties;++i){var p=group.property(i);
        record(indent+p.name);if(p.propertyType!==PropertyType.PROPERTY)dump(p,indent+'  ');}}
    function render(comp,name){
        var item=app.project.renderQueue.items.add(comp);item.applyTemplate('Best Settings');
        item.timeSpanStart=0;item.timeSpanDuration=1/24;
        var output=item.outputModule(1);output.applyTemplate('Lossless');output.file=new File(base.fsName+'/'+name+'.avi');
        app.project.renderQueue.render();
        if(item.status!==RQItemStatus.DONE||!output.file.exists||output.file.length===0)throw new Error('Render failed: '+name);
        record('RENDER '+name+' '+output.file.length);item.remove();
    }
    if(app.project.file!==null||app.project.numItems!==0){record('REFUSED: existing project is open.');return;}
    app.exitCode=1;app.setSavePreferencesOnQuit(false);app.beginSuppressDialogs();
    try {
        record('START AE '+app.version);
        var fixture=new File(root.fsName+'/artifacts/source.png');
        if(!fixture.exists)throw new Error('Run tests/compare_ae_controls.py --fixture first.');
        var source=app.project.importFile(new ImportOptions(fixture));
        var comp=app.project.items.addComp('4x4Tools v1.0 - controls verification',180,225,1,1,24);
        var layer=comp.layers.add(source);layer.property('ADBE Transform Group').property('ADBE Scale').setValue([28.125,62.5]);
        var fx=layer.property('ADBE Effect Parade').addProperty('4x4Tools DLSS5');
        if(!fx)throw new Error('Effect missing.');dump(fx,'CONTROL ');
        var required=['Neural style','Footage preset','Neural tone','Neural structure','Preserve original color',
            'Recover original texture','Artifact protection','Exposure (stops)','Wipe position','Apply enhancement'];
        for(var j=0;j<required.length;++j)if(!find(fx,required[j]))throw new Error('Missing control '+required[j]);
        app.project.bitsPerChannel=8;find(fx,'DLSS enhancement').setValue(1);render(comp,'original');
        find(fx,'DLSS enhancement').setValue(2);
        for(var style=1;style<=3;++style){find(fx,'Neural style').setValue(style);render(comp,'style-'+style);}
        find(fx,'Neural style').setValue(1);
        for(var look=2;look<=9;++look){find(fx,'Footage preset').setValue(look);render(comp,'look-'+look);}
        // Exercise restoration in deep and float application render paths.
        find(fx,'Footage preset').setValue(2);
        app.project.bitsPerChannel=16;render(comp,'natural-16');
        app.project.bitsPerChannel=32;render(comp,'natural-32');
        app.project.bitsPerChannel=8;find(fx,'Output').setValue(2);find(fx,'Wipe position').setValue(100);
        render(comp,'wipe-original');find(fx,'Wipe position').setValue(50);
        app.project.save(new File(base.fsName+'/Controls Verification.aep'));
        record('STAGE closing diagnostic project');app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
        record('PASS application renders and project cleanup. Compare AVI pixels separately.');app.exitCode=0;
        var latest=new File(artifacts.fsName+'/latest-ae-controls.txt');if(latest.open('w')){latest.write(base.fsName);latest.close();}
    }catch(e){record('FAIL '+e.toString()+' line='+e.line);}
    finally{app.endSuppressDialogs(false);}
})();
