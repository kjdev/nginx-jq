
ARG ALPINE_VERSION=3.15

FROM alpine:${ALPINE_VERSION} as nginx


FROM nginx as builder

WORKDIR /build
RUN apk --no-cache upgrade \
 && apk --no-cache add \
      curl \
      gcc \
      gd-dev \
      geoip-dev \
      jq-dev \
      libxslt-dev \
      linux-headers \
      make \
      musl-dev \
      nginx \
      openssl-dev \
      pcre-dev \
      perl-dev \
      zlib-dev \
 && nginx_version=$(nginx -v 2>&1 | sed 's/^[^0-9]*//') \
 && curl -sL -o nginx-${nginx_version}.tar.gz http://nginx.org/download/nginx-${nginx_version}.tar.gz \
 && tar -xf nginx-${nginx_version}.tar.gz \
 && mv nginx-${nginx_version} nginx

COPY config /build/
COPY src/ /build/src/

WORKDIR /build/nginx
RUN nginx_opt=$(nginx -V 2>&1 | tail -1 | sed -e "s/configure arguments://" -e "s| --add-dynamic-module=[^ ]*||g") \
 && ./configure \
      ${nginx_opt} \
      --add-dynamic-module=../ \
 && make \
 && mkdir -p /usr/lib/nginx/modules \
 && cp objs/ngx_http_jq_module.so /usr/lib/nginx/modules/ \
 && mkdir -p /etc/nginx/modules \
 && echo 'load_module "/usr/lib/nginx/modules/ngx_http_jq_module.so";' > /etc/nginx/modules/jq.conf \
 && nginx -t


FROM nginx

RUN apk --no-cache upgrade \
 && apk --no-cache add \
      jq \
      nginx \
 && sed -i 's/^user /#user /' /etc/nginx/nginx.conf

COPY --from=builder /usr/lib/nginx/modules/ngx_http_jq_module.so /usr/lib/nginx/modules/ngx_http_jq_module.so
COPY --from=builder /etc/nginx/modules/jq.conf /etc/nginx/modules/jq.conf

USER nginx
CMD ["nginx", "-g", "daemon off;"]
