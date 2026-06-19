// Ground-truth generator for com.mojang.blaze3d.vertex.VertexFormat (+ its nested
// VertexFormatElement / Mode / IndexType) from Minecraft 26.1.2, using the REAL
// decompiled classes. VertexFormat is pure 32-bit integer layout math: a builder
// accumulates element byte offsets (offset += element.byteSize(), plus explicit
// padding(n)), validates the final vertexSize is a multiple of 4 (Mth.isMultipleOf),
// and computes a 32-entry offsetsByElement[] table (offset per registered element
// id, or -1), an elementsMask (OR of 1<<id), getOffset(), contains(), and a hashCode
// of (elementsMask*31 + Arrays.hashCode(offsetsByElement)). No GL is touched by any
// of this — but we run the standard bootstrap defensively (this class needs none).
//
//   mcpp/tools/run_groundtruth.ps1 -Tool VertexFormatParity -Out mcpp/build/vertex_format.tsv
//
// We drive the canonical DefaultVertexFormat constants (the real renderer formats,
// reflected by name) plus a handful of synthetic formats built directly with the
// Builder to exercise padding() and every element type in odd orders. For each
// format we emit, in TSV rows dispatched by a leading TAG:
//
//   FMT   <name> <nElems> [<id> <index> <typeOrdinal> <normalized> <count> <offset>]* \
//         <vertexSize> <elementsMask> <hashCode> [<off_0> .. <off_31>] <toStringB64>
//      Each element descriptor carries the builder's actual running offset for that
//      element (offsets.get(index) — i.e. the value the private ctor stores), so C++
//      reconstructs the format via the exact (elements, names, offsets, vertexSize)
//      constructor with padding placed correctly regardless of where padding() sat.
//      vertexSize/elementsMask/hashCode and the full 32-int offsetsByElement[] are the
//      computed outputs to match. toStringB64 is base64("VertexFormat"+names).
//   ELT   <name> <id> <byteSize> <mask> <getOffset> <contains> <elementNameB64>
//      For every registered VertexFormatElement id 0..7, against format <name>:
//      byteSize()/mask() of the element, plus this format's getOffset(elt) and
//      contains(elt). elementNameB64 is base64(getElementName) or "-" if absent.
//   TYPE  <ordinal> <size> <nameB64>           VertexFormatElement.Type.size()/toString()
//   MODE  <ordinal> <primLen> <primStride> <connected> <ic0> <ic1> .. <ic_k>  for vertexCounts
//   IDX   <length> <ordinal> <bytes>           IndexType.least(length) + its .bytes

import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import com.mojang.blaze3d.vertex.VertexFormat;
import com.mojang.blaze3d.vertex.VertexFormatElement;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public class VertexFormatParity {
    static final java.io.PrintStream O = System.out;

    static String b64(String s) {
        return Base64.getEncoder().encodeToString(s.getBytes(StandardCharsets.UTF_8));
    }

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // VertexFormat needs no bootstrap; ignore.
        }

        // ── enumerate the canonical renderer formats (reflect every public static
        //    VertexFormat field of DefaultVertexFormat, in declaration order) ──
        Map<String, VertexFormat> formats = new LinkedHashMap<>();
        for (Field f : DefaultVertexFormat.class.getDeclaredFields()) {
            if (f.getType() == VertexFormat.class
                    && java.lang.reflect.Modifier.isStatic(f.getModifiers())) {
                f.setAccessible(true);
                formats.put(f.getName(), (VertexFormat) f.get(null));
            }
        }

        // ── plus synthetic Builder formats to exercise padding() and odd orders ──
        formats.put("SYN_pad_after_pos",
                VertexFormat.builder().add("Position", VertexFormatElement.POSITION).padding(4).build());
        formats.put("SYN_color_first",
                VertexFormat.builder()
                        .add("Color", VertexFormatElement.COLOR)
                        .add("Position", VertexFormatElement.POSITION)
                        .build());
        formats.put("SYN_uv1_uv2_only",
                VertexFormat.builder()
                        .add("UV1", VertexFormatElement.UV1)
                        .add("UV2", VertexFormatElement.UV2)
                        .build());
        formats.put("SYN_normal_pad3",
                VertexFormat.builder()
                        .add("Normal", VertexFormatElement.NORMAL)
                        .padding(1)
                        .build());
        formats.put("SYN_linewidth_only",
                VertexFormat.builder()
                        .add("LineWidth", VertexFormatElement.LINE_WIDTH)
                        .build());
        formats.put("SYN_all_elements",
                VertexFormat.builder()
                        .add("Position", VertexFormatElement.POSITION)
                        .add("Color", VertexFormatElement.COLOR)
                        .add("UV0", VertexFormatElement.UV0)
                        .add("UV1", VertexFormatElement.UV1)
                        .add("UV2", VertexFormatElement.UV2)
                        .add("Normal", VertexFormatElement.NORMAL)
                        .padding(1)
                        .add("LineWidth", VertexFormatElement.LINE_WIDTH)
                        .build());

        // reflection handles for private fields we read to reconstruct in C++
        Field fElements = VertexFormat.class.getDeclaredField("elements");
        fElements.setAccessible(true);
        Field fNames = VertexFormat.class.getDeclaredField("names");
        fNames.setAccessible(true);
        Field fVertexSize = VertexFormat.class.getDeclaredField("vertexSize");
        fVertexSize.setAccessible(true);

        Method mGetOffsets = VertexFormat.class.getMethod("getOffsetsByElement");
        Method mGetOffset = VertexFormat.class.getMethod("getOffset", VertexFormatElement.class);
        Method mContains = VertexFormat.class.getMethod("contains", VertexFormatElement.class);
        Method mMask = VertexFormat.class.getMethod("getElementsMask");
        Method mGetVertexSize = VertexFormat.class.getMethod("getVertexSize");
        Method mElementName = VertexFormat.class.getMethod("getElementName", VertexFormatElement.class);

        // all registered elements id 0..7 (POSITION..LINE_WIDTH)
        List<VertexFormatElement> allElts = new ArrayList<>();
        for (int id = 0; id < 8; id++) {
            VertexFormatElement e = VertexFormatElement.byId(id);
            if (e != null) allElts.add(e);
        }

        for (Map.Entry<String, VertexFormat> e : formats.entrySet()) {
            String name = e.getKey();
            VertexFormat vf = e.getValue();

            @SuppressWarnings("unchecked")
            List<VertexFormatElement> elements = (List<VertexFormatElement>) fElements.get(vf);
            @SuppressWarnings("unchecked")
            List<String> names = (List<String>) fNames.get(vf);
            int vertexSize = (int) fVertexSize.get(vf);
            int[] offs = (int[]) mGetOffsets.invoke(vf);
            int mask = (int) mMask.invoke(vf);
            int hash = vf.hashCode();

            StringBuilder sb = new StringBuilder();
            sb.append("FMT\t").append(name).append('\t').append(elements.size());
            for (VertexFormatElement el : elements) {
                int id = (int) VertexFormatElement.class.getMethod("id").invoke(el);
                int idx = (int) VertexFormatElement.class.getMethod("index").invoke(el);
                Object type = VertexFormatElement.class.getMethod("type").invoke(el);
                int typeOrd = ((Enum<?>) type).ordinal();
                boolean norm = (boolean) VertexFormatElement.class.getMethod("normalized").invoke(el);
                int count = (int) VertexFormatElement.class.getMethod("count").invoke(el);
                // the builder's actual running offset for this element == offs[id]
                int elOffset = offs[id];
                sb.append('\t').append(id).append('\t').append(idx).append('\t')
                  .append(typeOrd).append('\t').append(norm ? 1 : 0).append('\t').append(count)
                  .append('\t').append(elOffset);
            }
            sb.append('\t').append((int) mGetVertexSize.invoke(vf)); // == vertexSize
            sb.append('\t').append(mask);
            sb.append('\t').append(hash);
            for (int i = 0; i < 32; i++) sb.append('\t').append(offs[i]);
            sb.append('\t').append(b64(vf.toString()));
            O.println(sb.toString());

            // per-element ELT rows for all 8 registered elements
            for (VertexFormatElement el : allElts) {
                int id = (int) VertexFormatElement.class.getMethod("id").invoke(el);
                int byteSize = (int) VertexFormatElement.class.getMethod("byteSize").invoke(el);
                int eMask = (int) VertexFormatElement.class.getMethod("mask").invoke(el);
                int getOff = (int) mGetOffset.invoke(vf, el);
                boolean contains = (boolean) mContains.invoke(vf, el);
                String enB64;
                try {
                    enB64 = b64((String) mElementName.invoke(vf, el));
                } catch (java.lang.reflect.InvocationTargetException ex) {
                    enB64 = "-"; // IllegalArgumentException: element not in this format
                }
                O.println("ELT\t" + name + "\t" + id + "\t" + byteSize + "\t" + eMask
                        + "\t" + getOff + "\t" + (contains ? 1 : 0) + "\t" + enB64);
            }
        }

        // ── VertexFormatElement.Type: size() + toString() for every ordinal ──
        Class<?> typeCls = Class.forName("com.mojang.blaze3d.vertex.VertexFormatElement$Type");
        Object[] typeConsts = typeCls.getEnumConstants();
        Method mTypeSize = typeCls.getMethod("size");
        for (int i = 0; i < typeConsts.length; i++) {
            Object tc = typeConsts[i];
            int size = (int) mTypeSize.invoke(tc);
            O.println("TYPE\t" + i + "\t" + size + "\t" + b64(tc.toString()));
        }

        // ── VertexFormat.Mode: fields + indexCount(vertexCount) for a battery ──
        Class<?> modeCls = Class.forName("com.mojang.blaze3d.vertex.VertexFormat$Mode");
        Object[] modeConsts = modeCls.getEnumConstants();
        Field fPrimLen = modeCls.getField("primitiveLength");
        Field fPrimStride = modeCls.getField("primitiveStride");
        Field fConnected = modeCls.getField("connectedPrimitives");
        Method mIndexCount = modeCls.getMethod("indexCount", int.class);
        int[] vertexCounts = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 100, 1000, 65535, 65536, 262144};
        for (int i = 0; i < modeConsts.length; i++) {
            Object mc = modeConsts[i];
            int pl = fPrimLen.getInt(mc);
            int ps = fPrimStride.getInt(mc);
            boolean conn = fConnected.getBoolean(mc);
            StringBuilder sb = new StringBuilder();
            sb.append("MODE\t").append(i).append('\t').append(pl).append('\t').append(ps)
              .append('\t').append(conn ? 1 : 0);
            for (int vc : vertexCounts) {
                int ic = (int) mIndexCount.invoke(mc, vc);
                sb.append('\t').append(vc).append('\t').append(ic);
            }
            O.println(sb.toString());
        }

        // ── VertexFormat.IndexType.least(length) — the SHORT/INT crossover at 65536 ──
        Class<?> idxCls = Class.forName("com.mojang.blaze3d.vertex.VertexFormat$IndexType");
        Method mLeast = idxCls.getMethod("least", int.class);
        Field fBytes = idxCls.getField("bytes");
        int[] lengths = {0, 1, 100, 65535, 65536, 65537, 131072, 1000000, 16777216};
        for (int len : lengths) {
            Object it = mLeast.invoke(null, len);
            int ord = ((Enum<?>) it).ordinal();
            int bytes = fBytes.getInt(it);
            O.println("IDX\t" + len + "\t" + ord + "\t" + bytes);
        }
    }
}
