// The teardown surface of a GENERATED view plugin, checked the way ui-host
// actually reaches it: load the plugin, then resolve everything BY NAME
// through the meta-object.
//
// Why by name and not by calling the class directly: the host has no header
// for a module's generated plugin. It holds a QObject* from QPluginLoader and
// does QMetaObject::invokeMethod(obj, "aboutToUnload", Q_RETURN_ARG(int, rc)).
// When the meta-method is absent that call returns false and the host moves on
// -- which is indistinguishable from a view answering "Synchronous, nothing to
// wait for". So a test that linked the plugin class and called
// p.aboutToUnload() directly would still pass with the hook stripped from the
// meta-object. This one cannot: it only ever knows the strings.
//
// Three properties, each failing with its own sentence:
//   1. aboutToUnload() is an INVOKABLE method returning int (the host reads it
//      back with Q_RETURN_ARG(int) and must not need the SDK enum to do it);
//   2. unloadFinished() exists as a SIGNAL (a view that answers Asynchronous
//      with no way to say it is done would be refused the wait);
//   3. the completion actually ARRIVES -- the fixture backend finishes on a
//      later turn of the event loop, so this passes only if the generated
//      trampoline's QUEUED emission works.

#include <QCoreApplication>
#include <QDebug>
#include <QMetaMethod>
#include <QMetaObject>
#include <QPluginLoader>
#include <QTimer>

#include <cstdio>

static int g_failures = 0;

static void fail(const QString& why)
{
    qCritical().noquote() << "FAIL:" << why;
    ++g_failures;
}

// Receives unloadFinished() by NAME, the same way the host connects to it.
class Watcher : public QObject {
    Q_OBJECT
public:
    bool fired = false;
public Q_SLOTS:
    void onFinished()
    {
        fired = true;
        QCoreApplication::quit();
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        qCritical() << "usage: ui_plugin_metaobject_check <plugin-path>";
        return 2;
    }

    QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
    QObject* plugin = loader.instance();
    if (!plugin) {
        qCritical().noquote() << "FAIL: could not load plugin:" << loader.errorString();
        return 1;
    }

    const QMetaObject* mo = plugin->metaObject();

    // The evidence, printed whether or not anything fails: the method table the
    // host sees. A reviewer should be able to read the teardown surface off the
    // test log without rerunning anything.
    std::printf("--- QMetaObject method table for %s ---\n", mo->className());
    for (int i = 0; i < mo->methodCount(); ++i) {
        const QMetaMethod m = mo->method(i);
        const char* kind = "?";
        switch (m.methodType()) {
        case QMetaMethod::Signal:      kind = "SIGNAL"; break;
        case QMetaMethod::Slot:        kind = "SLOT";   break;
        case QMetaMethod::Method:      kind = "METHOD"; break;
        case QMetaMethod::Constructor: kind = "CTOR";   break;
        }
        const char* ret = (m.typeName() && *m.typeName()) ? m.typeName() : "void";
        std::printf("  [%2d] %-7s %-26s -> %s\n", i, kind,
                    m.methodSignature().constData(), ret);
    }
    std::printf("--- end method table ---\n");
    std::fflush(stdout);

    // 1. aboutToUnload() -- present, invokable, int-returning.
    const int auIdx = mo->indexOfMethod("aboutToUnload()");
    if (auIdx < 0) {
        fail(QStringLiteral(
            "the generated plugin has no aboutToUnload() meta-method. ui-host resolves "
            "this by name; absent, invokeMethod returns false and every teardown of "
            "every view silently skips its chance to finish."));
    } else {
        const QMetaMethod au = mo->method(auIdx);
        if (au.methodType() != QMetaMethod::Method)
            fail(QStringLiteral("aboutToUnload() is not a Q_INVOKABLE method (methodType %1)")
                     .arg(int(au.methodType())));
        if (qstrcmp(au.typeName(), "int") != 0)
            fail(QStringLiteral("aboutToUnload() must return int, not '%1' -- the host reads "
                                "it with Q_RETURN_ARG(int) and has no SDK enum")
                     .arg(QString::fromLatin1(au.typeName())));
    }

    // 2. unloadFinished() -- present, and a SIGNAL.
    const int ufIdx = mo->indexOfSignal("unloadFinished()");
    if (ufIdx < 0) {
        fail(QStringLiteral(
            "the generated plugin has no unloadFinished() SIGNAL. A view that answers "
            "Asynchronous would have no way to report completion, so the host would "
            "wait out the full grace period on every teardown."));
    }

    if (g_failures > 0)
        return 1;

    // 3. Drive it exactly as the host does, and require the completion to land.
    Watcher watcher;
    QObject::connect(plugin, SIGNAL(unloadFinished()), &watcher, SLOT(onFinished()));

    int rc = -1;
    if (!QMetaObject::invokeMethod(plugin, "aboutToUnload", Q_RETURN_ARG(int, rc))) {
        fail(QStringLiteral("QMetaObject::invokeMethod(\"aboutToUnload\") returned false -- "
                            "this is the exact call ui-host makes."));
        return 1;
    }
    // 1 == LogosShutdown::Asynchronous. The fixture backend defers its
    // completion, so anything else means the hook never reached the backend.
    if (rc != 1) {
        fail(QStringLiteral("aboutToUnload() answered %1; the fixture backend returns "
                            "Asynchronous (1), so the SFINAE helper did not reach it")
                 .arg(rc));
        return 1;
    }

    // Bounded: if the queued emission never arrives this must FAIL, not hang.
    QTimer::singleShot(5000, &app, &QCoreApplication::quit);
    app.exec();

    if (!watcher.fired) {
        fail(QStringLiteral(
            "unloadFinished() never arrived. The fixture backend completed on a later "
            "turn of the event loop, so the generated trampoline's QUEUED emission is "
            "what did not work -- the host would have waited out the grace period."));
        return 1;
    }

    std::printf("OK: aboutToUnload/unloadFinished present, int-returning, and the "
                "queued completion arrived.\n");
    return 0;
}

#include "main.moc"
