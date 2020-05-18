package com.zozo.ztr_android


import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import kotlin.math.max

object LabelCreator {

    private val labelNamePaint = Paint().apply {
        isAntiAlias = true
        textSize = 34f
    }
    private val valuePaint = Paint().apply {
        isAntiAlias = true
        textSize = 60f
    }
    private val unitPaint = Paint().apply {
        isAntiAlias = true
        textSize = 30f
    }
    private val _unitText = "cm"
    private val _unitTextBounds = Rect().apply {
        unitPaint.getTextBounds(_unitText, 0, _unitText.length, this)
    }
    private val _labelNameMargin = Margin(0, 0, 6, 12)
    private val _valueTextMargin = Margin(0, 12, 0, 9)
    private val _unitTextMargin = Margin(15, 0, 15, 0)

    @JvmStatic
    fun create(
            type: Int,
            value: Float
    ): ZsLabelTexture {

        val labelName = MeasurementType.valueOf(type).labelName
        val valueText = String.format("%.1f", value * 100f)

        val labelNameBounds = Rect()
        labelNamePaint.getTextBounds(labelName, 0, labelName.length, labelNameBounds)
        val labelX = _labelNameMargin.left.toFloat()
        val labelY = (_labelNameMargin.top + labelNameBounds.height()).toFloat()

        val valueTextBounds = Rect()
        valuePaint.getTextBounds(valueText, 0, valueText.length, valueTextBounds)
        val valueX = _valueTextMargin.left.toFloat()
        val valueY = (_labelNameMargin.height() + labelNameBounds.height() + _valueTextMargin.top + valueTextBounds.height()).toFloat()

        val unitX = (_valueTextMargin.width() + valueTextBounds.width() + _unitTextMargin.left).toFloat()

        val width = max(labelNameBounds.width() + _labelNameMargin.width(),
                _valueTextMargin.width() + valueTextBounds.width() + _unitTextMargin.width() + _unitTextBounds.width())
        val height = _labelNameMargin.height() + labelNameBounds.height() + _valueTextMargin.height() + valueTextBounds.height()

        val bmp = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bmp)
        canvas.drawText(labelName, labelX, labelY, labelNamePaint)
        canvas.drawText(valueText, valueX, valueY, valuePaint)
        canvas.drawText(_unitText, unitX, valueY, unitPaint)

        val result = ZsLabelTexture(width, height, IntArray(width * height))
        bmp.getPixels(result.data, 0, width, 0, 0, width, height)
        bmp.recycle()
        return result
    }
}

class ZsLabelTexture(
        val width: Int,
        val height: Int,
        val data: IntArray
)

internal class Margin(val left: Int, val top: Int, val right: Int, val bottom: Int) {
    fun width() = left + right
    fun height() = top + bottom
}

enum class MeasurementType(
        val labelName: String
) {
    None(""),
    Outline(""),
    SoleGirth(""),
    Length("足長"),
    BallWidth("足幅"),
    BallGirth("足囲"),
    HeelWidth("かかと幅"),
    InstepHeight("足甲高さ"),
    ArchHeight(""),
    ShortHeelGirth(""),
    LongHeelGirth(""),
    ArchLength(""),
    BigToeAngle(""),
    PinkyToeAngle("");

    companion object {
        fun valueOf(value: Int): MeasurementType = values().firstOrNull { it.ordinal == value } ?: None
    }
}
