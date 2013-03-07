/*
 *   render_lib.fd    � TEK neoscientists
 *   v27.0
 */

#pragma libcall RenderBase TurboFillMem 1e 10803
#pragma libcall RenderBase TurboCopyMem 24 09803
#pragma libcall RenderBase CreateRMHandlerA 2a 901
#pragma libcall RenderBase DeleteRMHandler 30 801
#pragma libcall RenderBase AllocRenderMem 36 0802
#pragma libcall RenderBase FreeRenderMem 3c 09803
#pragma libcall RenderBase AllocRenderVec 42 0802
#pragma libcall RenderBase FreeRenderVec 48 801
#pragma libcall RenderBase CreateHistogramA 4e 901
#pragma libcall RenderBase DeleteHistogram 54 801
#pragma libcall RenderBase QueryHistogram 5a 0802
#pragma libcall RenderBase AddRGB 60 10803
#pragma libcall RenderBase AddRGBImageA 66 A109805
#pragma libcall RenderBase AddChunkyImageA 6c BA109806
#pragma libcall RenderBase ExtractPaletteA 72 A09804
#pragma libcall RenderBase RenderA 78 BA910806
#pragma libcall RenderBase Planar2ChunkyA 7e A93210807
#pragma libcall RenderBase Chunky2RGBA 84 BA910806
#pragma libcall RenderBase Chunky2BitMapA 8a A5493210809
#pragma libcall RenderBase CreateScaleEngineA 90 9321005
#pragma libcall RenderBase DeleteScaleEngine 96 801
#pragma libcall RenderBase ScaleA 9c BA9804
#pragma libcall RenderBase ConvertChunkyA a2 CBA109807
#pragma libcall RenderBase CreatePenTableA a8 CBA109807
#pragma libcall RenderBase CreatePaletteA ae 901
#pragma libcall RenderBase DeletePalette b4 801
#pragma libcall RenderBase ImportPaletteA ba A09804
#pragma libcall RenderBase ExportPaletteA c0 A9803
#pragma libcall RenderBase CountRGB c6 0802
#pragma libcall RenderBase BestPen cc 0802
#pragma libcall RenderBase FlushPalette d2 801
#pragma libcall RenderBase SortPaletteA d8 90803
#pragma libcall RenderBase AddHistogramA de A9803
#pragma libcall RenderBase ScaleOrdinate e4 21003
#pragma libcall RenderBase CreateHistogramPointerArray ea 801
#pragma libcall RenderBase CountHistogram f0 801
#pragma libcall RenderBase CreateMapEngineA f6 9802
#pragma libcall RenderBase DeleteMapEngine fc 801
#pragma libcall RenderBase MapRGBArrayA 102 BA109806
#pragma libcall RenderBase RGBArrayDiversityA 108 910804
#pragma libcall RenderBase ChunkyArrayDiversityA 10e A109805
#pragma libcall RenderBase MapChunkyArrayA 114 CB10A9807
#pragma libcall RenderBase InsertAlphaChannelA 11a A910805
#pragma libcall RenderBase ExtractAlphaChannelA 120 A910805
#pragma libcall RenderBase ApplyAlphaChannelA 126 A910805
#pragma libcall RenderBase MixRGBArrayA 12c A2910806
#pragma libcall RenderBase AllocRenderVecClear 132 0802
#pragma libcall RenderBase CreateAlphaArrayA 138 910804
#pragma libcall RenderBase MixAlphaChannelA 13e BA109806
#pragma libcall RenderBase TintRGBArrayA 144 A93210807
#pragma libcall RenderBase GetPaletteAttrs 14a 0802
#pragma libcall RenderBase RemapArrayA 150 BA910806

#pragma tagcall RenderBase CreateRMHandler 2a 901
#pragma tagcall RenderBase CreateHistogram 4e 901
#pragma tagcall RenderBase AddRGBImage 66 A109805
#pragma tagcall RenderBase AddChunkyImage 6c BA109806
#pragma tagcall RenderBase ExtractPalette 72 A09804
#pragma tagcall RenderBase Render 78 BA910806
#pragma tagcall RenderBase Planar2Chunky 7e A93210807
#pragma tagcall RenderBase Chunky2RGB 84 BA910806
#pragma tagcall RenderBase Chunky2BitMap 8a A5493210809
#pragma tagcall RenderBase CreateScaleEngine 90 9321005
#pragma tagcall RenderBase Scale 9c BA9804
#pragma tagcall RenderBase ConvertChunky a2 CBA109807
#pragma tagcall RenderBase CreatePenTable a8 CBA109807
#pragma tagcall RenderBase CreatePalette ae 901
#pragma tagcall RenderBase ImportPalette ba A09804
#pragma tagcall RenderBase ExportPalette c0 A9803
#pragma tagcall RenderBase SortPalette d8 90803
#pragma tagcall RenderBase AddHistogram de A9803
#pragma tagcall RenderBase CreateMapEngine f6 9802
#pragma tagcall RenderBase MapRGBArray 102 BA109806
#pragma tagcall RenderBase RGBArrayDiversity 108 910804
#pragma tagcall RenderBase ChunkyArrayDiversity 10e A109805
#pragma tagcall RenderBase MapChunkyArray 114 CB10A9807
#pragma tagcall RenderBase InsertAlphaChannel 11a A910805
#pragma tagcall RenderBase ExtractAlphaChannel 120 A910805
#pragma tagcall RenderBase ApplyAlphaChannel 126 A910805
#pragma tagcall RenderBase MixRGBArray 12c A2910806
#pragma tagcall RenderBase CreateAlphaArray 138 910804
#pragma tagcall RenderBase MixAlphaChannel 13e BA109806
#pragma tagcall RenderBase TintRGBArray 144 A93210807
#pragma tagcall RenderBase RemapArray 150 BA910806
