package com.cz.android.media.ffmpeg.audio.view;

import com.cz.android.media.ffmpeg.audio.utils.Util;

/**
 * @author Created by cz
 * @date 2020/10/22 3:13 PM
 * @email bingo110@126.com
 */
public class DefaultTextFormatter implements TextFormatter {
    @Override
    public String formatText(long value) {
        return Util.getStringForTime(value);
    }
}
