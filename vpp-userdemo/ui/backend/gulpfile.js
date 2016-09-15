var gulp            = require('gulp');
var concat          = require('gulp-concat');
var rename          = require('gulp-rename');
var cleanCSS        = require('gulp-clean-css');
var uglify          = require('gulp-uglify');
var mainBowerFiles  = require('main-bower-files');
var gulpFilter      = require('gulp-filter');
var imagemin        = require('gulp-imagemin');
var flatten         = require('gulp-flatten');
var jshint          = require('gulp-jshint');
var sass            = require('gulp-sass');
var watch           = require('gulp-watch');
var wiredep         = require('wiredep').stream;
var series          = require('stream-series');
var inject          = require('gulp-inject');
var clean           = require('gulp-clean');
var livereload      = require('gulp-livereload');

var paths = {
    source: {
        bower:      '../frontend/bower_components',
        images:     '../frontend/assets/images/**',
        js:         '../frontend/**/*.js',
        html:       '../frontend/**/*.html',
        index:      '../frontend/index.html',
        sass:       '../frontend/assets/sass/*.scss',
        fonts:      '../frontend/assets/fonts/**'
    },
    target: {
        dev:        '../dist.dev',
        prod:       '../dist.prod',
        imagesDev:  '../dist.dev/images',
        imagesProd: '../dist.prod/images',
        htmlDev:    '../dist.dev/html',
        htmlProd:   '../dist.prod/html',
        cssDev:     '../dist.dev/css',
        cssProd:    '../dist.prod/cssProd',
        jsDev:      '../dist.dev/js',
        jsProd:     '../dist.prod/js',
        vendorDev:  '../dist.dev/vendor',
        vendorProd: '../dist.prod/vendor',
        indexDev:   '../dist.dev/html/index.html',
        indexProd:  '../dist.prod/html/index.html',
        fontsDev:   '../dist.dev/fonts',
        fontsProd:  '../dist.prod/fonts'
    }
}

gulp.task('bower', function() {

    var jsFilter    = gulpFilter(paths.source.bower+'/**/*.js', {restore: true})
    var cssFilter   = gulpFilter(paths.source.bower+'/**/*.css', {restore: true})

    return gulp.src(mainBowerFiles())
    .pipe(jsFilter)
    .pipe(concat('vendor.js'))
    .pipe(gulp.dest(paths.target.vendorDev))
    .pipe(rename('vendor.min.js'))
    .pipe(gulp.dest(paths.target.vendorProd))
    .pipe(jsFilter.restore)
    .pipe(cssFilter)
    .pipe(concat('vendor.css'))
    .pipe(gulp.dest(paths.target.vendorDev))
    .pipe(rename('vendor.min.css'))
    .pipe(cleanCSS())
    .pipe(gulp.dest(paths.target.vendorProd))
    .pipe(cssFilter.restore)
    .pipe(livereload());
});


gulp.task('images', function() {
    return gulp.src(paths.source.images)
    .pipe(imagemin({ progressive: true }))
    .pipe(flatten())
    .pipe(gulp.dest(paths.target.imagesDev))
    .pipe(gulp.dest(paths.target.imagesProd))
    .pipe(livereload());
});

gulp.task('lint', function() {
    return gulp.src(['!'+paths.source.bower+'/**', paths.source.js])
    .pipe(jshint())
    .pipe(jshint.reporter('default'));
});

gulp.task('html', function() {
    return gulp.src(['!'+paths.source.bower+'/**', '!'+paths.source.index, paths.source.html])
    .pipe(flatten())
    .pipe(gulp.dest(paths.target.htmlDev))
    .pipe(gulp.dest(paths.target.htmlProd))
    .pipe(livereload());
});

gulp.task('fonts', function() {
    return gulp.src([paths.source.fonts])
    .pipe(flatten())
    .pipe(gulp.dest(paths.target.fontsDev))
    .pipe(gulp.dest(paths.target.fontsProd));
});

gulp.task('js', function() {
    return gulp.src(['!'+paths.source.bower+'/**', paths.source.js])
    .pipe(concat('app.js'))
    .pipe(gulp.dest(paths.target.jsDev))
    .pipe(rename('app.min.js'))
    .pipe(gulp.dest(paths.target.jsProd))
    .pipe(livereload());
});

gulp.task('sass', function() {
    return gulp.src(paths.source.sass)
    .pipe(sass()).on('error', onError)
    .pipe(gulp.dest(paths.target.cssDev))
    .pipe(gulp.dest(paths.target.cssProd))
    .pipe(livereload());
});

gulp.task('watch', function() {
    livereload.listen();
    gulp.watch([paths.source.images], ['images']);
    gulp.watch([paths.source.html], ['html']);
    gulp.watch([paths.source.bower], ['bower']);
    gulp.watch(['!'+paths.source.bower+'/**', paths.source.js], ['lint', 'js']);
    gulp.watch(paths.source.sass, ['sass']);
});

gulp.task('clean', function() {
    gulp.src(paths.target.dev, {read: false})
    .pipe(clean({force: true}))
    gulp.src(paths.target.prod, {read: false})
    .pipe(clean({force: true}))
});

gulp.task('injectDev', ['bower', 'sass', 'js'], function () {
    var target = gulp.src(paths.source.index);
    var sources = gulp.src([paths.target.vendorDev+'/*.js', paths.target.vendorDev+'/*.css', paths.target.cssDev+'/*.css', paths.target.jsDev+'/*.js'], {read: false});
    return target.pipe(inject(sources, {addRootSlash : false, 
        transform : function(filePath, file, i, length){
            var newPath = filePath.replace('dist.dev/', '');
            if(filePath.match(/.css/)){
                return '<link rel="stylesheet" href="'+newPath+'">'
            }
            else
                return '<script src="'+newPath+'"></script>';
        }}))
    .pipe(gulp.dest(paths.target.htmlDev));
});

gulp.task('injectProd', ['bower', 'sass', 'js'], function () {
    var target = gulp.src(paths.source.index);
    var sources = gulp.src([paths.target.vendorProd+'/*.js', paths.target.vendorProd+'/*.css', paths.target.cssProd+'/*.css', paths.target.jsProd+'/*.js'], {read: false});
    return target.pipe(inject(sources, {addRootSlash : false, 
        transform : function(filePath, file, i, length){
            var newPath = filePath.replace('dist.prod/', '');
            if(filePath.match(/.css/)){
                return '<link rel="stylesheet" href="'+newPath+'">'
            }
            else
                return '<script src="'+newPath+'"></script>';
        }}))
    .pipe(gulp.dest(paths.target.htmlProd));
});

function onError(err) {
    console.log(err);
    this.emit('end');
}

gulp.task('default', ['bower', 'lint', 'js', 'sass', 'images', 'fonts', 'html', 'injectDev', 'injectProd', 'watch']);



