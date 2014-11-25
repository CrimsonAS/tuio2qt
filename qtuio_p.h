#ifndef QTUIO_P_H
#define QTUIO_P_H

// TODO: we should communicate errors ('read past end of data') vs empty strings
inline QByteArray qt_readOscString(const QByteArray &data, quint32 &pos)
{
    QByteArray re;
    int end = data.indexOf('\0', pos);
    if (end < 0) {
        pos = data.size();
        return re;
    }

    re = data.mid(pos, end - pos);

    // Skip additional NULL bytes at the end of the string to make sure the
    // total number of bits a multiple of 32 bits ("OSC-string" in the
    // specification).
    end += 4 - ((end - pos) % 4);

    pos = end;
    return re;
}


#endif
