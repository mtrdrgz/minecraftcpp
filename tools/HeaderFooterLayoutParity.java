// Ground truth for net.minecraft.client.gui.layouts.HeaderAndFooterLayout.arrangeElements. Drives the
// REAL layout with a headless Screen subclass (constructed via the (Minecraft,Font,Component) ctor with
// nulls — those args are only stored, never dereferenced; (Minecraft)null/(Font)null only LINK the
// classes, no static init) whose public width/height fields are set directly. Children are
// SpacerElements added to header/footer/contents; after arrangeElements their final getX/getY are read.
//
//   tools/run_groundtruth.ps1 -Tool HeaderFooterLayoutParity -Out mcpp/build/header_footer_layout.tsv
//
// Rows:
//   LAYOUT <s> <screenW> <screenH> <headerH> <footerH>  <getHeaderHeight> <getFooterHeight> <getContentHeight>
//   HPOS   <s> <i> <getX> <getY>   (header child)
//   FPOS   <s> <i> <getX> <getY>   (footer child)
//   CPOS   <s> <i> <getX> <getY>   (content child)

import java.util.List;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.Font;
import net.minecraft.client.gui.components.events.GuiEventListener;
import net.minecraft.client.gui.layouts.HeaderAndFooterLayout;
import net.minecraft.client.gui.layouts.SpacerElement;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;

public class HeaderFooterLayoutParity {
    static final java.io.PrintStream O = System.out;

    static final class S extends Screen {
        S() { super((Minecraft) null, (Font) null, Component.empty()); }
        @Override public List<? extends GuiEventListener> children() { return List.of(); }
    }

    // Each scenario: {screenW, screenH, headerH, footerH}
    static final int[][] CFG = {
        {320, 240, 33, 33},
        {640, 480, 48, 60},
        {200, 150, 33, 33},
        {400, 300, 20, 20},
        {500, 400, 33, 33}
    };
    // header / footer / content child sizes, as flat {w,h, w,h, ...} per scenario.
    static final int[][] HDR = {
        {200, 9}, {300, 20, 100, 40}, {180, 33}, {}, {250, 9}
    };
    static final int[][] FTR = {
        {150, 20}, {200, 30}, {180, 33}, {100, 10}, {250, 20, 60, 9}
    };
    static final int[][] CON = {
        {220, 40}, {400, 100, 380, 150}, {190, 120}, {220, 40, 50, 200}, {220, 260}
    };

    static SpacerElement[] add(int[] flat, java.util.function.Function<SpacerElement, SpacerElement> adder) {
        SpacerElement[] arr = new SpacerElement[flat.length / 2];
        for (int i = 0; i < arr.length; i++) {
            SpacerElement sp = new SpacerElement(flat[2 * i], flat[2 * i + 1]);
            arr[i] = adder.apply(sp);
        }
        return arr;
    }

    public static void main(String[] args) {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        for (int s = 0; s < CFG.length; s++) {
            int[] c = CFG[s];
            S screen = new S();
            HeaderAndFooterLayout hfl = new HeaderAndFooterLayout(screen, c[2], c[3]);
            screen.width = c[0];
            screen.height = c[1];
            SpacerElement[] hdr = add(HDR[s], sp -> hfl.addToHeader(sp));
            SpacerElement[] ftr = add(FTR[s], sp -> hfl.addToFooter(sp));
            SpacerElement[] con = add(CON[s], sp -> hfl.addToContents(sp));
            hfl.arrangeElements();

            O.println("LAYOUT\t" + s + "\t" + c[0] + "\t" + c[1] + "\t" + c[2] + "\t" + c[3]
                    + "\t" + hfl.getHeaderHeight() + "\t" + hfl.getFooterHeight() + "\t" + hfl.getContentHeight());
            for (int i = 0; i < hdr.length; i++)
                O.println("HPOS\t" + s + "\t" + i + "\t" + hdr[i].getX() + "\t" + hdr[i].getY());
            for (int i = 0; i < ftr.length; i++)
                O.println("FPOS\t" + s + "\t" + i + "\t" + ftr[i].getX() + "\t" + ftr[i].getY());
            for (int i = 0; i < con.length; i++)
                O.println("CPOS\t" + s + "\t" + i + "\t" + con[i].getX() + "\t" + con[i].getY());
        }
    }
}
