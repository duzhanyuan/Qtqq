#include "qqtextedit.h"

#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QFile>
#include <QDebug>

#include "core/qqsetting.h"

QQTextEdit::QQTextEdit(QWidget *parent) : QTextEdit(parent)
{

}

QQTextEdit::~QQTextEdit()
{
    QMovie *mov = NULL;
    foreach (mov, id_mov_hash_.values())
    {
        mov->deleteLater();
        mov = NULL;
    }
}

void QQTextEdit::appendDocument(const QTextDocument *doc)
{
    QTextCursor cursor(this->document());

    QTextBlockFormat format;
    format.setLeftMargin(8);
    format.setTopMargin(5);
    cursor.movePosition(QTextCursor::End);
    cursor.mergeBlockFormat(format);

    cursor.insertHtml(doc->toHtml());
}

void QQTextEdit::insertNameLine(const QString &name, QColor color)
{
    QTextBlockFormat block_format;
    block_format.setTopMargin(5);

    QTextCharFormat char_format;
    char_format.setForeground(QBrush(color));

    QTextCursor cursor(this->document());

    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock(block_format, char_format);

    cursor.insertText(name);

    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
}

void QQTextEdit::insertWord(const QString &text, QFont font, QColor color, int size)
{
    QTextCursor cursor(this->document());
    cursor.movePosition(QTextCursor::End);

    QTextBlockFormat block_format;
    block_format.setLeftMargin(8);
    block_format.setTopMargin(5);
    block_format.setLineHeight(5, QTextBlockFormat::LineDistanceHeight);

    QTextCharFormat char_format;
    char_format.setForeground(color);
    char_format.setFont(font);
    char_format.setFontPointSize(size);

    cursor.setBlockFormat(block_format);
    cursor.setBlockCharFormat(char_format);

    cursor.insertText(text);
}

void QQTextEdit::insertQQFace(const QString &face_id)
{
    QTextDocument *doc = document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::End);

    QString path = QQSettings::instance()->resourcePath() + "/qqface/default/" + face_id + ".gif";
    QImage img(path);
    QUrl url(kQQFacePre+face_id);
    doc->addResource(QTextDocument::ImageResource, url, img);
    cursor.insertImage(kQQFacePre+face_id);

    if(file_ids_.contains(kQQFacePre+face_id)){ //ͬһ��gif ʹ��ͬһ��movie
        return;
    }else{
       file_ids_.append(kQQFacePre+face_id);
    }

   QMovie* movie = new QMovie();
   movie->setFileName(path);
   movie->setSpeed(70);
   movie->setCacheMode(QMovie::CacheAll);

   id_mov_hash_.insert(kQQFacePre+face_id, movie);

   //��֡ʱˢ��
   connect(movie, SIGNAL(frameChanged(int)), this, SLOT(animate(int)));
   movie->start();
}

void QQTextEdit::insertImgProxy(const QString &unique_id)
{
    QTextDocument *doc = document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::End);

    QImage img(QQSettings::instance()->resourcePath() + "/loading/loading.gif");
    QUrl url(unique_id);
    doc->addResource(QTextDocument::ImageResource, url, img);
    cursor.insertImage(unique_id);

    QMovie* movie = new QMovie();
    movie->setSpeed(70);
    movie->setFileName(QQSettings::instance()->resourcePath() + "/loading/loading.gif");
    movie->setCacheMode(QMovie::CacheAll);

    id_mov_hash_.insert(unique_id, movie);

    //��֡ʱˢ��
    connect(movie, SIGNAL(frameChanged(int)), this, SLOT(animate(int)));
    movie->start();
}

void QQTextEdit::replaceIdToName(QString id, QString name)
{
    QTextDocument *doc = this->document();
    QTextCursor cursor = doc->find(id, QTextCursor());

    if (cursor.isNull())
        return;

    cursor.insertText(name);
}

void QQTextEdit::setRealImg(const QString &unique_id, const QString &path)
{
    file_ids_.append(unique_id);

    QFile file(path);
    file.open(QIODevice::ReadOnly);
    QByteArray data = file.readAll();
    file.close();

    QImage real_img;
    real_img.loadFromData(data);

    document()->addResource(QTextDocument::ImageResource,   //�滻ͼƬΪ��ǰ֡
                            QUrl(unique_id), real_img);
    this->update();
    QMovie *mov = id_mov_hash_.value(unique_id);
    mov->stop();
    mov->setFileName(path);
    if (mov->isValid())
        qDebug()<<'valid'<<endl;

    mov->setCacheMode(QMovie::CacheAll);
    connect(mov, SIGNAL(frameChanged(int)), this, SLOT(animate(int)));
    mov->start();
}

void QQTextEdit::addAnimaImg(const QString &unique_id, const QVariant &resource, QMovie *mov)
{
    this->document()->addResource(QTextDocument::ImageResource, QUrl(unique_id), resource);
    disconnect(mov);
    connect(mov, SIGNAL(frameChanged(int)), this, SLOT(animate(int)));
    file_ids_.append(unique_id);
    id_mov_hash_.insert(unique_id, mov);
}

void QQTextEdit::animate(int)
{
    if (QMovie* movie = qobject_cast<QMovie*>(sender()))
    {
        document()->addResource(QTextDocument::ImageResource,   //�滻ͼƬΪ��ǰ֡
                                id_mov_hash_.key(movie), movie->currentPixmap());

        if (movie->currentFrameNumber() == movie->frameCount())
                    qApp->processEvents();

        setLineWrapColumnOrWidth(lineWrapColumnOrWidth()); // ..ˢ����ʾ
    }
}